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
import datetime
import re
import json
import seaborn as sns
import plotly
import scipy
import sklearn

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
    explanation: str = Field(..., description="Explanation of the generated code")

class AnalysisResult(BaseModel):
    """Result of log analysis execution."""
    success: bool = Field(..., description="Whether the analysis was successful")
    output: str = Field(..., description="Text output from code execution")
    error: str = Field(default="", description="Error message if execution failed")
    figures: List[str] = Field(default_factory=list, description="List of base64-encoded figures")
    dataframes: List[Dict[str, Any]] = Field(default_factory=list, description="List of DataFrame summaries")
    result: Optional[Dict[str, Any]] = Field(default=None, description="The final analysis result")
    generated_code: Optional[str] = Field(default=None, description="The generated Python code")

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
            'datetime': datetime,
            're': re,
            'json': json,
            'seaborn': sns,  # For better visualizations
            'plotly': plotly,  # For interactive visualizations
            'scipy': scipy,  # For statistical analysis
            'sklearn': sklearn,  # For machine learning analysis
        }
        
        # Initialize the AI model for code generation
        self.model = OpenAIModel(api_key=api_key, model="gpt-4")
        
        # Initialize the code generation agent with type enforcement
        self.code_generation_agent = Agent(
            self.model,
            result_type=CodeGenerationResponse,
            system_prompt="""You are a Python code generation expert specializing in log analysis and data visualization.
Your task is to generate Python code that analyzes log data based on the user's request.

Guidelines:
1. Use the provided helper functions and data structures
2. Always include proper error handling
3. Generate visualizations when relevant
4. Store the final result in a 'result' dictionary
5. Use pandas and matplotlib/seaborn for data manipulation and visualization
6. Include comments explaining the code
7. Follow Python best practices and PEP 8 style guide

The code should:
- Be safe to execute in a sandboxed environment
- Handle missing or malformed data gracefully
- Use appropriate data types and conversions
- Generate clear and informative visualizations
- Provide meaningful analysis results"""
        )
        
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
            
            # Log analysis specific helpers
            'parse_timestamp': lambda ts: pd.to_datetime(ts, errors='coerce'),
            'extract_patterns': lambda text, pattern: re.findall(pattern, text),
            'calculate_stats': lambda series: {
                'mean': series.mean(),
                'median': series.median(),
                'std': series.std(),
                'min': series.min(),
                'max': series.max(),
                'count': series.count(),
                'unique': series.nunique()
            },
            'detect_anomalies': lambda series, threshold=2: {
                'mean': series.mean(),
                'std': series.std(),
                'anomalies': series[abs(series - series.mean()) > threshold * series.std()]
            },
            'group_by_time': lambda df, col, freq='H': df.set_index(col).groupby(pd.Grouper(freq=freq)),
            'create_heatmap': lambda df, x, y, values: pd.pivot_table(df, values=values, index=y, columns=x, aggfunc='count'),
            
            # Visualization helpers
            'plot_timeseries': lambda df, x, y, title=None: self._plot_timeseries(df, x, y, title),
            'plot_distribution': lambda series, title=None: self._plot_distribution(series, title),
            'plot_correlation': lambda df, cols, title=None: self._plot_correlation(df, cols, title),
            'plot_heatmap': lambda df, x, y, values, title=None: self._plot_heatmap(df, x, y, values, title),
            
            # Safe versions of certain functions
            'plt': plt,
            'sns': sns,
            'plotly': plotly,
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

    def _plot_timeseries(self, df: pd.DataFrame, x: str, y: str, title: Optional[str] = None) -> None:
        """Helper to create a time series plot."""
        plt.figure(figsize=(12, 6))
        plt.plot(df[x], df[y])
        if title:
            plt.title(title)
        plt.xlabel(x)
        plt.ylabel(y)
        plt.grid(True, alpha=0.3)
        
    def _plot_distribution(self, series: pd.Series, title: Optional[str] = None) -> None:
        """Helper to create a distribution plot."""
        plt.figure(figsize=(10, 6))
        sns.histplot(series, kde=True)
        if title:
            plt.title(title)
        plt.grid(True, alpha=0.3)
        
    def _plot_correlation(self, df: pd.DataFrame, cols: List[str], title: Optional[str] = None) -> None:
        """Helper to create a correlation heatmap."""
        if len(cols) < 2:
            raise ValueError("Need at least 2 columns for correlation analysis")
            
        # Select only numeric columns
        numeric_df = df[cols].select_dtypes(include=[np.number])
        if len(numeric_df.columns) < 2:
            raise ValueError("Need at least 2 numeric columns for correlation analysis")
            
        plt.figure(figsize=(10, 8))
        sns.heatmap(numeric_df.corr(), annot=True, cmap='coolwarm', center=0)
        if title:
            plt.title(title)
            
    def _plot_heatmap(self, df: pd.DataFrame, x: str, y: str, values: str, title: Optional[str] = None) -> None:
        """Helper to create a heatmap from pivot table."""
        pivot_table = pd.pivot_table(df, values=values, index=y, columns=x, aggfunc='count')
        plt.figure(figsize=(12, 8))
        sns.heatmap(pivot_table, annot=True, fmt='g', cmap='YlOrRd')
        if title:
            plt.title(title)

    async def analyze_logs(self, analysis_task: str) -> AnalysisResult:
        """Generate and execute code to analyze logs based on a task description.
        
        Args:
            analysis_task: Description of the analysis task
            
        Returns:
            Result of the executed code
        """
        self.console.print(f"[bold blue]Generating code for analysis task:[/] {analysis_task}")
        
        try:
            # Generate the analysis code using AI
            code_response = await self.code_generation_agent.run(
                CodeGenerationRequest(
                    task_description=analysis_task,
                    available_data={
                        "dataframes": self._get_available_data(),
                        "functions": self._get_available_functions()
                    },
                    requirements=[
                        "Use pandas and matplotlib for analysis and visualization",
                        "Store the final result in a 'result' variable as a dictionary",
                        "Include appropriate error handling",
                        "Generate visualizations when relevant",
                        "Use the provided helper functions for data access"
                    ]
                )
            )
            
            # Execute the generated code
            self.console.print("[bold blue]Executing analysis code...[/]")
            execution_result = self.execute_code(code_response.code)
            
            if execution_result['success']:
                self.console.print("[bold green]Analysis completed successfully[/]")
            else:
                self.console.print(f"[bold red]Analysis failed:[/] {execution_result['error']}")
            
            # Create the analysis result with type enforcement
            return AnalysisResult(
                success=execution_result['success'],
                output=execution_result['output'],
                error=execution_result['error'],
                figures=execution_result['figures'],
                dataframes=execution_result['dataframes'],
                result=execution_result['result'],
                generated_code=code_response.code
            )
            
        except Exception as e:
            self.console.print(f"[bold red]Error during code generation:[/] {str(e)}")
            return AnalysisResult(
                success=False,
                output="",
                error=f"Code generation failed: {str(e)}",
                figures=[],
                dataframes=[],
                result=None,
                generated_code=None
            )

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