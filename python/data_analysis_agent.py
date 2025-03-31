#!/usr/bin/env python3
import os
import sys
import io
import contextlib
import traceback
import pandas as pd
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
from typing import Dict, Any, List, Optional, Union
import base64
import duckdb
from rich.console import Console
from pydantic import BaseModel, Field
from pydantic_ai import Agent, RunContext
from pydantic_ai.models.openai import OpenAIModel

# Force matplotlib to use Agg backend (non-interactive, good for web)
matplotlib.use('Agg')

class CodeGenerationRequest(BaseModel):
    """Request for generating analysis code."""
    task_description: str = Field(..., description="Description of the analysis task")
    available_data: Dict[str, str] = Field(..., description="Description of available data and functions")
    requirements: List[str] = Field(default_factory=list, description="Specific requirements for the analysis")

class CodeGenerationResponse(BaseModel):
    """Response containing generated analysis code."""
    code: str = Field(..., description="The generated Python code")
    explanation: str = Field(..., description="Explanation of what the code does")
    required_imports: List[str] = Field(default_factory=list, description="Required Python imports")
    expected_output: str = Field(..., description="Description of expected output")

class DataAnalysisAgent:
    """Agent for executing data analysis code in a sandboxed environment."""
    
    def __init__(self, duckdb_conn=None, log_table="log_entries", templates_table="log_templates",
                 api_key: Optional[str] = None):
        """Initialize the DataAnalysisAgent.
        
        Args:
            duckdb_conn: An existing DuckDB connection to use
            log_table: Name of the table containing log entries
            templates_table: Name of the table containing log templates
            api_key: Optional API key for OpenAI
        """
        self.console = Console()
        self.duckdb_conn = duckdb_conn
        self.log_table = log_table
        self.templates_table = templates_table
        
        # Set up a local connection if none provided
        if self.duckdb_conn is None:
            self.duckdb_conn = duckdb.connect('logai.db')
        
        # Setup execution context with allowed modules
        self.globals = {
            'pd': pd,
            'np': np,
            'plt': plt,
            'matplotlib': matplotlib,
            'io': io,
        }
        
        # Initialize the AI model for code generation
        self.model = OpenAIModel(api_key=api_key, model="gpt-4")
        
    def load_logs_as_dataframe(self) -> pd.DataFrame:
        """Load log data into a Pandas DataFrame for analysis."""
        return self.duckdb_conn.execute(f"SELECT * FROM {self.log_table}").fetchdf()
        
    def load_templates_as_dataframe(self) -> pd.DataFrame:
        """Load template data into a Pandas DataFrame for analysis."""
        return self.duckdb_conn.execute(f"SELECT * FROM {self.templates_table}").fetchdf()
    
    def execute_code(self, code: str) -> Dict[str, Any]:
        """Execute Python code in a sandbox and return the results.
        
        Args:
            code: Python code to execute
            
        Returns:
            Dict containing:
                - 'success': Boolean indicating success
                - 'output': Text output from code execution
                - 'error': Error message if execution failed
                - 'figures': List of base64-encoded figures if any were generated
                - 'dataframes': List of DataFrames if any were created/returned
        """
        # Prepare capture of stdout
        stdout_capture = io.StringIO()
        
        # Prepare result object
        result = {
            'success': False,
            'output': '',
            'error': '',
            'figures': [],
            'dataframes': [],
            'result': None
        }
        
        # Setup the execution context with data access helpers
        local_context = {
            'get_logs': self.load_logs_as_dataframe,
            'get_templates': self.load_templates_as_dataframe,
            'execute_query': lambda query: self.duckdb_conn.execute(query).fetchdf(),
            'result': None,  # Will store the final result
            'plot_to_base64': self._plot_to_base64,
            # Safe versions of certain functions
            'plt': plt,
        }
        
        # Add our globals to the execution context
        exec_globals = dict(self.globals)
        exec_globals.update(local_context)
        
        # Execute the code in the sandbox with stdout capturing
        try:
            with contextlib.redirect_stdout(stdout_capture):
                # Execute the user code
                exec(code, exec_globals)
                
                # Check for figures
                if plt.get_fignums():
                    for fig_num in plt.get_fignums():
                        fig = plt.figure(fig_num)
                        img_data = self._plot_to_base64(fig)
                        result['figures'].append(img_data)
                    plt.close('all')
                
                # Check for DataFrames in the local context
                for var_name, var_value in exec_globals.items():
                    if isinstance(var_value, pd.DataFrame) and var_name not in ['get_logs', 'get_templates']:
                        # Store limited info about DataFrames to avoid huge responses
                        result['dataframes'].append({
                            'name': var_name,
                            'shape': var_value.shape,
                            'columns': var_value.columns.tolist(),
                            'head': var_value.head(5).to_dict(orient='records')
                        })
                
                # Grab the result if set
                if 'result' in exec_globals and exec_globals['result'] is not None:
                    result['result'] = exec_globals['result']
                
            # Set success and output
            result['success'] = True
            result['output'] = stdout_capture.getvalue()
            
        except Exception as e:
            # Handle any errors during execution
            result['success'] = False
            result['error'] = f"{str(e)}\n{traceback.format_exc()}"
        
        return result
    
    def _plot_to_base64(self, fig) -> str:
        """Convert a matplotlib figure to a base64 encoded string."""
        img_data = io.BytesIO()
        fig.savefig(img_data, format='png', bbox_inches='tight')
        img_data.seek(0)
        base64_str = base64.b64encode(img_data.getvalue()).decode()
        return base64_str

    async def generate_analysis_code(self, task_description: str) -> str:
        """Generate Python code for the analysis task using AI.
        
        Args:
            task_description: Description of the analysis task
            
        Returns:
            Generated Python code as a string
        """
        # Dynamically inspect available data and functions
        available_data = {}
        
        # Inspect DataFrame columns if available
        try:
            logs_df = self.load_logs_as_dataframe()
            available_data["logs"] = {
                "type": "DataFrame",
                "columns": logs_df.columns.tolist(),
                "description": "Log entries data"
            }
        except Exception as e:
            self.console.print(f"[yellow]Warning: Could not inspect logs DataFrame: {e}[/]")
            
        try:
            templates_df = self.load_templates_as_dataframe()
            available_data["templates"] = {
                "type": "DataFrame",
                "columns": templates_df.columns.tolist(),
                "description": "Log templates data"
            }
        except Exception as e:
            self.console.print(f"[yellow]Warning: Could not inspect templates DataFrame: {e}[/]")
        
        # Inspect available functions in the execution context
        available_functions = {}
        for name, func in self.globals.items():
            if callable(func):
                # Get function signature and docstring
                import inspect
                sig = inspect.signature(func)
                doc = inspect.getdoc(func) or "No documentation available"
                available_functions[name] = {
                    "type": "function",
                    "signature": str(sig),
                    "description": doc
                }
        
        # Add our helper functions
        available_functions.update({
            "get_logs": {
                "type": "function",
                "signature": "() -> pd.DataFrame",
                "description": "Load log entries as a DataFrame"
            },
            "get_templates": {
                "type": "function",
                "signature": "() -> pd.DataFrame",
                "description": "Load templates as a DataFrame"
            },
            "execute_query": {
                "type": "function",
                "signature": "(query: str) -> pd.DataFrame",
                "description": "Execute SQL queries and return results as DataFrame"
            },
            "plot_to_base64": {
                "type": "function",
                "signature": "(fig) -> str",
                "description": "Convert matplotlib figures to base64 strings"
            }
        })
        
        # Create the request
        request = CodeGenerationRequest(
            task_description=task_description,
            available_data={
                "dataframes": available_data,
                "functions": available_functions
            },
            requirements=[
                "Use pandas and matplotlib for analysis and visualization",
                "Store the final result in a 'result' variable as a dictionary",
                "Include appropriate error handling",
                "Generate visualizations when relevant",
                "Use the provided helper functions for data access"
            ]
        )
        
        # Create the agent for code generation
        agent = Agent(self.model, tools=[self.execute_code])
        
        # Generate the code
        response = await agent.run(request)
        
        # Extract the code from the response
        if isinstance(response, CodeGenerationResponse):
            return response.code
        else:
            raise ValueError("Unexpected response format from code generation")
    
    async def analyze_logs(self, analysis_task: str) -> Dict[str, Any]:
        """Generate and execute code to analyze logs based on a task description.
        
        Args:
            analysis_task: Description of the analysis task
            
        Returns:
            Result of the executed code
        """
        self.console.print(f"[bold blue]Generating code for analysis task:[/] {analysis_task}")
        
        try:
            # Generate the analysis code using AI
            code = await self.generate_analysis_code(analysis_task)
            
            # Execute the generated code
            self.console.print("[bold blue]Executing analysis code...[/]")
            execution_result = self.execute_code(code)
            
            if execution_result['success']:
                self.console.print("[bold green]Analysis completed successfully[/]")
            else:
                self.console.print(f"[bold red]Analysis failed:[/] {execution_result['error']}")
            
            # Add the generated code to the result
            execution_result['generated_code'] = code
            
            return execution_result
            
        except Exception as e:
            self.console.print(f"[bold red]Error during code generation:[/] {str(e)}")
            return {
                'success': False,
                'error': f"Code generation failed: {str(e)}",
                'generated_code': None
            }

# For CLI testing
if __name__ == "__main__":
    import asyncio
    
    async def test_agent():
        agent = DataAnalysisAgent()
        task = "Count logs by level and show distribution"
        result = await agent.analyze_logs(task)
        print(f"Success: {result['success']}")
        if result['figures']:
            print(f"Generated {len(result['figures'])} figures")
        if 'result' in result and result['result']:
            print(f"Analysis result: {result['result']}")
    
    asyncio.run(test_agent()) 