#!/usr/bin/env python3
import sys
import json
import argparse
from typing import List, Dict, Any, Optional, Union, Callable, Literal, Type, TypeVar
from dataclasses import dataclass
from pydantic import BaseModel, Field

# Import pydantic-ai
from pydantic_ai import Agent, RunContext
from pydantic_ai.models.openai import OpenAIModel
from pydantic_ai.models.anthropic import AnthropicModel
from pydantic_ai.models.gemini import GeminiModel
from pydantic_ai.models.openai import OpenAIModel
from pydantic_ai.providers.openai import OpenAIProvider

# Optional imports for Ollama
try:
    from pydantic_ai.providers.openai import OpenAIProvider
    HAS_OLLAMA = True
except ImportError:
    HAS_OLLAMA = False

# Helper for CLI styling
from rich.console import Console
from rich.markdown import Markdown
from rich.panel import Panel
from rich.prompt import Prompt
from rich.syntax import Syntax

# Data handling
import pandas as pd
import duckdb

# Import the C++ module directly
try:
    import logai_cpp
    HAS_CPP_MODULE = True
except ImportError:
    print("Warning: LogAI C++ module not found. Some functionality will be limited.")
    HAS_CPP_MODULE = False

# Import our data analysis agent
try:
    from data_analysis_agent import DataAnalysisAgent
    HAS_DATA_ANALYSIS = True
except ImportError:
    print("Warning: DataAnalysisAgent module not found. Advanced analytics will be limited.")
    HAS_DATA_ANALYSIS = False

# Import specialized agents
from specialized_agents import SpecializedAgents

# Define models for our agent tools
class LogTemplate(BaseModel):
    """A log template with its associated information."""
    template_id: int = Field(..., description="Unique identifier for the template")
    template: str = Field(..., description="The log template text")
    count: int = Field(..., description="Number of logs matching this template")
    
class LogAttributes(BaseModel):
    """Attributes extracted from logs for a given template."""
    template_id: int = Field(..., description="The template ID these attributes belong to")
    attributes: Dict[str, List[Any]] = Field(..., description="Dictionary of attribute name to list of values")

class SearchResult(BaseModel):
    """Result from searching logs."""
    logs: List[Dict[str, Any]] = Field(..., description="List of matching log entries")
    count: int = Field(..., description="Total count of matching logs")

class QueryResult(BaseModel):
    """Result from querying the database."""
    columns: List[str] = Field(..., description="Column names in the result")
    rows: List[List[Any]] = Field(..., description="Result rows from the query")
    
class TimeRange(BaseModel):
    """Time range information from logs."""
    start_time: str = Field(..., description="Start time of logs in ISO format")
    end_time: str = Field(..., description="End time of logs in ISO format")
    duration_seconds: float = Field(..., description="Duration in seconds")

class CountResult(BaseModel):
    """Result of counting occurrences."""
    total: int = Field(..., description="Total count of matched items")
    breakdown: Dict[str, int] = Field(default_factory=dict, description="Count breakdown by categories")

# Define agent dependencies and result models
@dataclass
class LogAIDependencies:
    """Dependencies for the LogAI agent."""
    log_file: Optional[str] = None
    is_initialized: bool = False
    cpp_wrapper: Any = None
    console: Any = None

class LogAnalysisStep(BaseModel):
    """A step in the log analysis process."""
    thought: str = Field(..., description="The agent's thinking process for this step")
    tool: str = Field(..., description="The tool to use for this step")
    tool_input: Dict[str, Any] = Field(..., description="The input parameters for the tool")

class LogAnalysisResult(BaseModel):
    """The final result of log analysis."""
    steps: List[LogAnalysisStep] = Field(..., description="Steps taken to analyze the logs")
    final_answer: str = Field(..., description="The final answer to the user's question")

class LogAnalysisRequest(BaseModel):
    """Request for log analysis."""
    query: str = Field(..., description="The natural language query to analyze")
    context: Dict[str, Any] = Field(default_factory=dict, description="Additional context for the analysis")

class LogAnalysisResponse(BaseModel):
    """Response from log analysis."""
    answer: str = Field(..., description="The final answer to the user's question")
    steps: List[LogAnalysisStep] = Field(..., description="Steps taken to analyze the logs")
    tools_used: List[str] = Field(..., description="List of tools used in the analysis")
    confidence: float = Field(..., description="Confidence score for the analysis")
    error: Optional[str] = Field(default=None, description="Error message if analysis failed")

# Default model mapping for different providers
DEFAULT_MODELS = {
    "openai": "gpt-4o",
    "gemini": "gemini-1.5-flash",
    "claude": "claude-3-sonnet-20240229",
    "ollama": "llama3"
}

# Provider Type
ProviderType = Literal["openai", "gemini", "claude", "ollama"]

# Main LogAI Agent class
class LogAIAgent:
    """LogAI Agent for analyzing logs with AI assistance."""
    
    def __init__(self, provider: Literal["openai", "gemini", "claude", "ollama"] = "openai", 
                 api_key: Optional[str] = None, 
                 model: Optional[str] = None,
                 host: Optional[str] = "http://localhost:11434"):
        """Initialize the LogAI Agent."""
        self.console = Console()
        self.provider = provider
        self.is_initialized = False
        self.log_file = None
        self.cpp_wrapper = logai_cpp
        self.api_key = api_key
        self.model = None
        self.host = host
        self.specialized_agents = None
        
        # Set up model based on provider
        self._setup_model(provider, model, api_key, host)
        
        # Initialize the main analysis agent with type enforcement
        self.analysis_agent = Agent(
            self.model,
            result_type=LogAnalysisResponse,
            system_prompt="""You are a log analysis expert specializing in analyzing and interpreting log data.
Your task is to help users understand their logs by answering questions and providing insights.

Guidelines:
1. Use available tools to gather relevant log data
2. Analyze patterns and relationships in the logs
3. Provide clear and concise explanations
4. Include supporting evidence from logs
5. Consider temporal relationships between events
6. Identify potential issues and their causes
7. Suggest relevant follow-up questions

The analysis should:
- Be thorough and well-reasoned
- Use appropriate tools for each step
- Provide confidence scores for conclusions
- Include relevant log excerpts as evidence
- Consider both direct and indirect relationships"""
        )
        
    def _setup_model(self, provider: str, model: Optional[str] = None, api_key: Optional[str] = None, host: Optional[str] = None):
        """Set up the appropriate AI model based on provider."""
        model_name = model or DEFAULT_MODELS.get(provider)
        
        if provider == "openai":
            from pydantic_ai.models.openai import OpenAIModel
            self.model = OpenAIModel(api_key=api_key, model=model_name)
        elif provider == "gemini":
            from pydantic_ai.models.gemini import GeminiModel
            self.model = GeminiModel(api_key=api_key, model=model_name)
        elif provider == "claude":
            from pydantic_ai.models.anthropic import AnthropicModel
            self.model = AnthropicModel(api_key=api_key, model=model_name)
        elif provider == "ollama" and HAS_OLLAMA:
            from pydantic_ai.providers.openai import OpenAIProvider
            # Configure Ollama with OpenAI-compatible provider
            self.model = OpenAIProvider(base_url=host, api_key="", model=model_name)
        else:
            raise ValueError(f"Unsupported provider: {provider}")

    def initialize(self, log_file: str, format: Optional[str] = None) -> bool:
        """Initialize the agent with a log file."""
        self.console.print(f"[bold blue]Initializing LogAI Agent with file:[/] {log_file}")
        
        try:
            # Create DuckDB connection
            import duckdb
            self.duckdb_conn = duckdb.connect('logai.db')
            
            # Create tables if they don't exist
            self.duckdb_conn.execute("""
                CREATE TABLE IF NOT EXISTS log_entries (
                    id INTEGER PRIMARY KEY,
                    timestamp VARCHAR,
                    level VARCHAR,
                    message VARCHAR,
                    template_id VARCHAR
                )
            """)
            
            self.duckdb_conn.execute("""
                CREATE TABLE IF NOT EXISTS log_templates (
                    template_id VARCHAR PRIMARY KEY,
                    template TEXT,
                    count INTEGER
                )
            """)
            
            # Parse the log file using the C++ wrapper
            parsed_logs = self.cpp_wrapper.parse_log_file(log_file, format or "")
            if not parsed_logs:
                self.console.print("[bold red]Failed to parse log file.[/]")
                return False
            
            # Load the parsed logs into DuckDB
            self._load_logs_to_duckdb(parsed_logs)
            
            # Extract templates
            templates = self._extract_templates_from_logs(parsed_logs)
            
            # Store templates in DuckDB
            self._store_templates_in_duckdb(templates)
            
            # Store templates in Milvus if available
            try:
                self._store_templates_in_milvus(templates)
            except Exception as e:
                self.console.print(f"[bold yellow]Warning: Failed to store templates in Milvus: {str(e)}[/]")
            
            # Initialize specialized agents
            self.specialized_agents = SpecializedAgents(self.duckdb_conn, self.api_key)
            
            self.log_file = log_file
            self.is_initialized = True
            
            self.console.print("[bold green]✓[/] Log file parsed successfully")
            self.console.print("[bold green]✓[/] Templates extracted and stored")
            self.console.print("[bold green]✓[/] Attributes stored in DuckDB")
            self.console.print("[bold green]✓[/] Specialized agents initialized")
            
            return True
        except Exception as e:
            self.console.print(f"[bold red]Error initializing agent:[/] {str(e)}")
            return False

    def _load_logs_to_duckdb(self, parsed_logs: List[Dict[str, Any]]) -> None:
        """Load parsed logs into DuckDB."""
        # Create a Pandas DataFrame from the parsed logs
        import pandas as pd
        
        # Extract relevant fields
        records = []
        for i, log in enumerate(parsed_logs):
            record = {
                'id': i,
                'timestamp': log.get('timestamp'),
                'level': log.get('level'),
                'message': log.get('message'),
                'template_id': log.get('template_id', ''),
                'body': log.get('body')
            }
            records.append(record)
        
        # Create DataFrame
        df = pd.DataFrame(records)
        
        # Insert into DuckDB
        self.duckdb_conn.execute("DELETE FROM log_entries")
        self.duckdb_conn.register('df', df) # Register DataFrame instead of INSERT
        self.duckdb_conn.execute("INSERT INTO log_entries SELECT * FROM df")
        self.duckdb_conn.unregister('df') # Unregister after use

        self.console.print(f"[bold green]✓[/] Loaded {len(records)} log entries into DuckDB")

    def _extract_templates_from_logs(self, parsed_logs: List[Dict[str, Any]]) -> Dict[str, Dict[str, Any]]:
        """Extract templates from parsed logs."""
        templates = {}
        
        # Count occurrences of each template
        for log in parsed_logs:
            template = log.get('template', '')
            if not template:
                continue
            
            template_id = str(hash(template) % 10000000)
            
            if template_id in templates:
                templates[template_id]['count'] += 1
            else:
                templates[template_id] = {
                    'template_id': template_id,
                    'template': template,
                    'count': 1
                }
        
        return templates

    def _store_templates_in_duckdb(self, templates: Dict[str, Dict[str, Any]]) -> None:
        """Store templates in DuckDB."""
        import pandas as pd
        
        # Create DataFrame from templates
        template_records = list(templates.values())
        df = pd.DataFrame(template_records)
        
        # Insert into DuckDB
        self.duckdb_conn.execute("DELETE FROM log_templates")
        self.duckdb_conn.execute("INSERT INTO log_templates SELECT * FROM df")
        self.duckdb_conn.unregister('df') # Unregister after use
        
        self.console.print(f"[bold green]✓[/] Stored {len(template_records)} templates in DuckDB")

    def _store_templates_in_milvus(self, templates: Dict[str, Dict[str, Any]]) -> None:
        """Store templates in Milvus."""
        # Initialize Milvus connection
        if not self.cpp_wrapper.init_milvus():
            raise Exception("Failed to initialize Milvus connection")
        
        # Store each template in Milvus
        stored_count = 0
        failed_count = 0
        
        for template_id, template_data in templates.items():
            # Generate embedding
            template_text = template_data['template']
            embedding = self.cpp_wrapper.generate_template_embedding(template_text)
            
            if not embedding:
                failed_count += 1
                continue
            
            # Store in Milvus using the Python Milvus client
            success = self.cpp_wrapper.insert_template(
                template_id=template_id,
                template=template_text,
                count=template_data['count'],
                embedding=embedding
            )
            
            if success:
                stored_count += 1
            else:
                failed_count += 1
        
        self.console.print(f"[bold green]✓[/] Stored {stored_count} templates in Milvus, failed: {failed_count}")

    def initialize_data_analysis_agent(self) -> bool:
        """Initialize the DataAnalysisAgent for advanced analytics."""
        if not HAS_DATA_ANALYSIS:
            self.console.print("[bold yellow]Warning: DataAnalysisAgent not available - advanced analytics disabled.[/]")
            return False
            
        try:
            self.data_analysis_agent = DataAnalysisAgent(duckdb_conn=self.duckdb_conn)
            self.console.print("[bold green]✓[/] Data Analysis Agent initialized")
            return True
        except Exception as e:
            self.console.print(f"[bold red]Error initializing Data Analysis Agent:[/] {str(e)}")
            return False

    def execute_query(self, query: str) -> Dict[str, Any]:
        """Execute a SQL query against the log data."""
        if not self.is_initialized:
            self.console.print("[bold red]Error: Agent not initialized. Call initialize() first.[/]")
            return {"columns": [], "rows": []}
        
        try:
            # Execute query directly in DuckDB
            result = self.duckdb_conn.execute(query).fetchdf()
            
            # Convert to dictionary format
            columns = result.columns.tolist()
            rows = result.values.tolist()
            
            return {
                "columns": columns,
                "rows": rows
            }
        except Exception as e:
            self.console.print(f"[bold red]Error executing query:[/] {str(e)}")
            return {"columns": [], "rows": []}

    # --- Tool Methods --- #

    def search_logs(self, query: str, limit: int = 100) -> Dict[str, Any]:
        """Search log messages containing a specific query string."""
        sql = f"""
            SELECT id, timestamp, level, message 
            FROM log_entries 
            WHERE message LIKE '%{query}%' 
            ORDER BY id DESC 
            LIMIT {limit}
        """
        result = self.execute_query(sql)
        # Convert to SearchResult format (optional, returning dict is okay too)
        return {
            "logs": [dict(zip(result['columns'], row)) for row in result['rows']],
            "count": len(result['rows']) # Note: This is count of returned, not total matching
        }

    def get_template(self, template_id: str) -> Optional[Dict[str, Any]]:
        """Get details for a specific log template."""
        sql = f"SELECT template_id, template, count FROM log_templates WHERE template_id = '{template_id}'"
        result = self.execute_query(sql)
        if result and result['rows']:
            return dict(zip(result['columns'], result['rows'][0]))
        return None

    def get_time_range(self) -> Dict[str, Any]:
        """Get the start and end time of the loaded logs."""
        sql = "SELECT MIN(timestamp) as start_time, MAX(timestamp) as end_time FROM log_entries"
        result = self.execute_query(sql)
        if result and result['rows'] and result['rows'][0][0] is not None:
            start, end = result['rows'][0]
            # TODO: Calculate duration accurately if needed
            return {"start_time": str(start), "end_time": str(end), "duration_seconds": 0.0}
        return {"start_time": "N/A", "end_time": "N/A", "duration_seconds": 0.0}

    def count_occurrences(self, pattern: str, group_by: Optional[str] = None) -> Dict[str, Any]:
        """Count occurrences of a pattern, optionally grouped."""
        where_clause = f"WHERE message LIKE '%{pattern}%'"
        
        if group_by and group_by in ['level', 'template_id']: # Add other valid group_by columns
            sql = f"""
                SELECT {group_by}, COUNT(*) as count
                FROM log_entries
                {where_clause}
                GROUP BY {group_by}
                ORDER BY count DESC
            """
            result = self.execute_query(sql)
            total = sum(row[1] for row in result['rows'])
            breakdown = {str(row[0]): row[1] for row in result['rows']}
        else:
            sql = f"SELECT COUNT(*) FROM log_entries {where_clause}"
            result = self.execute_query(sql)
            total = result['rows'][0][0] if result and result['rows'] else 0
            breakdown = {}
            
        return {"total": total, "breakdown": breakdown}

    def summarize_logs(self, time_range: Optional[Dict[str, str]] = None) -> Dict[str, Any]:
        """Provide a summary of logs (e.g., count by level)."""
        # Basic summary: count by level
        sql = "SELECT level, COUNT(*) as count FROM log_entries GROUP BY level ORDER BY count DESC"
        # TODO: Add time_range filtering if provided
        result = self.execute_query(sql)
        summary = {str(row[0]): row[1] for row in result['rows']} if result else {}
        return {"summary_by_level": summary}

    def filter_by_time(self, since: Optional[str] = None, until: Optional[str] = None, limit: int = 100) -> List[Dict[str, Any]]:
        """Filter logs by a time range."""
        conditions = []
        if since:
            conditions.append(f"timestamp >= '{since}'")
        if until:
            conditions.append(f"timestamp <= '{until}'")
        where_clause = f"WHERE {' AND '.join(conditions)}" if conditions else ""
        sql = f"""
            SELECT id, timestamp, level, message 
            FROM log_entries 
            {where_clause} 
            ORDER BY id DESC 
            LIMIT {limit}
        """
        result = self.execute_query(sql)
        return [dict(zip(result['columns'], row)) for row in result['rows']] if result else []

    def filter_by_level(self, levels: Optional[List[str]] = None, exclude_levels: Optional[List[str]] = None, limit: int = 100) -> List[Dict[str, Any]]:
        """Filter logs by level."""
        conditions = []
        if levels:
            level_list = ", ".join([f"'{l}'" for l in levels])
            conditions.append(f"level IN ({level_list})")
        if exclude_levels:
            exclude_list = ", ".join([f"'{l}'" for l in exclude_levels])
            conditions.append(f"level NOT IN ({exclude_list})")
        where_clause = f"WHERE {' AND '.join(conditions)}" if conditions else ""
        sql = f"""
            SELECT id, timestamp, level, message 
            FROM log_entries 
            {where_clause} 
            ORDER BY id DESC 
            LIMIT {limit}
        """
        result = self.execute_query(sql)
        return [dict(zip(result['columns'], row)) for row in result['rows']] if result else []

    def calculate_statistics(self) -> Dict[str, Any]:
        """Calculate basic statistics about the logs."""
        # Example: total count, time range, count by level
        stats = {}
        try:
            count_res = self.execute_query("SELECT COUNT(*) FROM log_entries")
            stats['total_logs'] = count_res['rows'][0][0] if count_res and count_res['rows'] else 0
            
            time_res = self.get_time_range()
            stats['time_range'] = time_res
            
            level_res = self.execute_query("SELECT level, COUNT(*) FROM log_entries GROUP BY level")
            stats['count_by_level'] = {str(row[0]): row[1] for row in level_res['rows']} if level_res else {}
        except Exception as e:
            self.console.print(f"[bold yellow]Warning calculating statistics: {e}[/]")
        return stats

    def get_trending_patterns(self, time_window: str = "hour") -> List[Dict[str, Any]]:
        """Identify trending log templates (placeholder)."""
        # This is complex. A simple placeholder: return top 5 templates by count.
        sql = "SELECT template_id, template, count FROM log_templates ORDER BY count DESC LIMIT 5"
        result = self.execute_query(sql)
        return [dict(zip(result['columns'], row)) for row in result['rows']] if result else []

    def analyze_logs(self, analysis_task: str) -> Dict[str, Any]:
        """Perform advanced log analysis using python code.
        
        This uses the DataAnalysisAgent to generate and execute Python code
        to analyze the logs based on the given task description. The result
        includes visualizations and detailed analytics.
        
        Args:
            analysis_task: Description of the analysis to perform
            
        Returns:
            Dict with analysis results, including any visualizations
        """
        if not self.is_initialized:
            return {"error": "Agent not initialized with log data"}
            
        if not hasattr(self, 'data_analysis_agent'):
            success = self.initialize_data_analysis_agent()
            if not success:
                return {"error": "Failed to initialize Data Analysis Agent"}
        
        self.console.print(f"[bold blue]Performing log analysis:[/] {analysis_task}")
        try:
            # Call the data analysis agent to perform the task
            # Since analyze_logs is now async, we need to run it in an event loop
            import asyncio
            analysis_result = asyncio.run(self.data_analysis_agent.analyze_logs(analysis_task))
            
            # Prepare a summary of the result
            summary = {
                "success": analysis_result.get("success", False),
                "analysis": analysis_result.get("result", {}).get("analysis", "No analysis available"),
                "has_visualizations": len(analysis_result.get("figures", [])) > 0,
                "visualization_count": len(analysis_result.get("figures", [])),
                "code_generated": True,
                "error": analysis_result.get("error", "")
            }
            
            # Include the full result for detailed access
            summary["full_result"] = analysis_result
            
            return summary
        except Exception as e:
            self.console.print(f"[bold red]Error during log analysis:[/] {str(e)}")
            return {"error": f"Analysis failed: {str(e)}"}

    # --- AI Chat Method --- #
    async def chat_query(self, query: str) -> LogAnalysisResponse:
        """Processes a natural language query using the AI model and available tools."""
        if not self.model:
            return LogAnalysisResponse(
                answer="Error: AI model not initialized.",
                steps=[],
                tools_used=[],
                confidence=0.0,
                error="AI model not initialized"
            )
        if not self.is_initialized:
            return LogAnalysisResponse(
                answer="Error: Agent not initialized with log data.",
                steps=[],
                tools_used=[],
                confidence=0.0,
                error="Agent not initialized with log data"
            )

        # Initialize data analysis agent if needed
        if not hasattr(self, 'data_analysis_agent') and HAS_DATA_ANALYSIS:
            self.initialize_data_analysis_agent()

        # Define the tools available to the AI
        tools = [
            self.search_logs,
            self.get_template,
            self.get_time_range,
            self.count_occurrences,
            self.summarize_logs,
            self.filter_by_time,
            self.filter_by_level,
            self.calculate_statistics,
            self.get_trending_patterns,
            self.execute_query,
        ]
        
        # Add specialized agent tools if available
        if self.specialized_agents:
            tools.extend([
                self.specialized_agents.analyze_causality,
                self.specialized_agents.analyze_user_impact,
                self.specialized_agents.analyze_service_dependencies,
                self.specialized_agents.get_event_context,
                self.specialized_agents.compare_events,
            ])
        
        # Add the analyze_logs tool if data analysis agent is available
        if hasattr(self, 'data_analysis_agent'):
            tools.append(self.analyze_logs)

        self.console.print(f"[bold blue]Processing chat query:[/] {query}")
        
        try:
            # Use the analysis agent to process the query
            response = await self.analysis_agent.run(
                LogAnalysisRequest(
                    query=query,
                    context={
                        "tools": tools,
                        "is_initialized": self.is_initialized,
                        "has_data_analysis": hasattr(self, 'data_analysis_agent'),
                        "has_specialized_agents": self.specialized_agents is not None
                    }
                )
            )
            
            return response
            
        except Exception as e:
            self.console.print(f"[bold red]Error processing query:[/] {str(e)}")
            return LogAnalysisResponse(
                answer="Error processing query.",
                steps=[],
                tools_used=[],
                confidence=0.0,
                error=str(e)
            )