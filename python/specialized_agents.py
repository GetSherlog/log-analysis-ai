#!/usr/bin/env python3
from typing import Dict, Any, List, Optional
from pydantic import BaseModel, Field
from pydantic_ai import Agent, RunContext
from pydantic_ai.models.openai import OpenAIModel
from pydantic_ai.models.anthropic import AnthropicModel
from pydantic_ai.models.gemini import GeminiModel
from rich.console import Console

# Specialized prompts for each agent type
CAUSAL_ANALYSIS_PROMPT = """You are a causal analysis expert specializing in log analysis. Your task is to identify potential causes of specific events in log data.

Guidelines:
1. Look for temporal patterns and sequences of events
2. Consider both direct and indirect causes
3. Pay attention to error messages and warnings
4. Analyze service dependencies and interactions
5. Consider system state changes and resource usage
6. Provide confidence scores based on evidence strength
7. Include supporting log entries for each cause

Output should include:
- List of potential causes with confidence scores
- Supporting evidence from logs
- Temporal relationships between events
- Impact assessment of each cause"""

USER_IMPACT_PROMPT = """You are a user impact analysis expert specializing in log analysis. Your task is to assess how many users were affected by specific events.

Guidelines:
1. Identify user identifiers in log entries
2. Track unique user sessions and interactions
3. Consider both direct and indirect user impact
4. Analyze impact duration and severity
5. Categorize users by type/role if possible
6. Consider geographical or service-specific impact
7. Provide clear metrics and breakdowns

Output should include:
- Total number of affected users
- Impact duration and timeline
- User breakdown by type/role
- Severity assessment"""

SERVICE_DEPENDENCY_PROMPT = """You are a service dependency analysis expert specializing in log analysis. Your task is to map and analyze service interactions and dependencies.

Guidelines:
1. Identify service-to-service calls
2. Map dependency chains and critical paths
3. Analyze service health and performance
4. Identify bottlenecks and failure points
5. Consider both synchronous and asynchronous interactions
6. Track service versions and compatibility
7. Analyze error propagation patterns

Output should include:
- Complete dependency graph
- Critical service paths
- Identified bottlenecks
- Service health metrics"""

EVENT_CONTEXT_PROMPT = """You are a context analysis expert specializing in log analysis. Your task is to provide comprehensive context around specific events.

Guidelines:
1. Analyze events before and after the target event
2. Consider system state and configuration changes
3. Track resource usage and performance metrics
4. Identify related events and patterns
5. Consider external factors and dependencies
6. Analyze error patterns and recovery attempts
7. Provide timeline of significant events

Output should include:
- Preceding events and their significance
- Following events and outcomes
- Related events and patterns
- System state changes"""

COMPARATIVE_ANALYSIS_PROMPT = """You are a comparative analysis expert specializing in log analysis. Your task is to compare different events or time periods.

Guidelines:
1. Identify key metrics for comparison
2. Analyze patterns and trends
3. Consider seasonal or cyclical factors
4. Compare error rates and types
5. Analyze performance differences
6. Consider system changes between periods
7. Provide statistical significance

Output should include:
- Key differences and similarities
- Impact comparison metrics
- Statistical analysis
- Trend identification"""

# Models for different types of analysis
class CausalAnalysisRequest(BaseModel):
    """Request for causal analysis."""
    event: str = Field(..., description="The event to analyze")
    time_window: str = Field(default="1h", description="Time window to look for causes")

class CausalAnalysisResult(BaseModel):
    """Result of causal analysis."""
    potential_causes: List[Dict[str, Any]] = Field(..., description="List of potential causes")
    confidence_scores: Dict[str, float] = Field(..., description="Confidence scores for each cause")
    supporting_evidence: List[Dict[str, Any]] = Field(..., description="Supporting log entries")

class UserImpactRequest(BaseModel):
    """Request for user impact analysis."""
    event: str = Field(..., description="The event to analyze")
    user_field: str = Field(default="user_id", description="Field containing user identifier")

class UserImpactResult(BaseModel):
    """Result of user impact analysis."""
    affected_users: int = Field(..., description="Number of affected users")
    impact_duration: str = Field(..., description="Duration of impact")
    user_breakdown: Dict[str, int] = Field(..., description="Breakdown by user type/role")

class ServiceDependencyRequest(BaseModel):
    """Request for service dependency analysis."""
    service: str = Field(..., description="The service to analyze")
    time_window: str = Field(default="1h", description="Time window for analysis")

class ServiceDependencyResult(BaseModel):
    """Result of service dependency analysis."""
    dependencies: List[Dict[str, Any]] = Field(..., description="List of service dependencies")
    critical_paths: List[List[str]] = Field(..., description="Critical service paths")
    bottlenecks: List[Dict[str, Any]] = Field(..., description="Identified bottlenecks")

class EventContextRequest(BaseModel):
    """Request for event context analysis."""
    event_id: str = Field(..., description="ID of the event to analyze")
    context_window: str = Field(default="5m", description="Time window for context")

class EventContextResult(BaseModel):
    """Result of event context analysis."""
    preceding_events: List[Dict[str, Any]] = Field(..., description="Events before the target")
    following_events: List[Dict[str, Any]] = Field(..., description="Events after the target")
    related_events: List[Dict[str, Any]] = Field(..., description="Related events")

class ComparativeAnalysisRequest(BaseModel):
    """Request for comparative analysis."""
    event1: str = Field(..., description="First event/period to compare")
    event2: str = Field(..., description="Second event/period to compare")
    metrics: List[str] = Field(default_factory=list, description="Metrics to compare")

class ComparativeAnalysisResult(BaseModel):
    """Result of comparative analysis."""
    differences: Dict[str, Any] = Field(..., description="Key differences found")
    similarities: Dict[str, Any] = Field(..., description="Key similarities found")
    impact_comparison: Dict[str, float] = Field(..., description="Impact comparison metrics")

class SpecializedAgents:
    """Collection of specialized AI agents for different types of log analysis."""
    
    def __init__(self, duckdb_conn, api_key: Optional[str] = None):
        """Initialize specialized agents with different models."""
        self.console = Console()
        self.duckdb_conn = duckdb_conn
        
        # Initialize different models for different tasks
        # Complex tasks use GPT-4
        self.causal_model = OpenAIModel(api_key=api_key, model="gpt-4")
        self.dependency_model = OpenAIModel(api_key=api_key, model="gpt-4")
        
        # Medium complexity tasks use Claude
        self.context_model = AnthropicModel(api_key=api_key, model="claude-3-sonnet-20240229")
        self.comparison_model = AnthropicModel(api_key=api_key, model="claude-3-sonnet-20240229")
        
        # Simpler tasks use Gemini
        self.impact_model = GeminiModel(api_key=api_key, model="gemini-1.5-flash")
        
        # Initialize agents with their respective models and return types
        self.causal_agent = Agent(
            self.causal_model,
            result_type=CausalAnalysisResult,
            system_prompt=CAUSAL_ANALYSIS_PROMPT
        )
        
        self.dependency_agent = Agent(
            self.dependency_model,
            result_type=ServiceDependencyResult,
            system_prompt=SERVICE_DEPENDENCY_PROMPT
        )
        
        self.context_agent = Agent(
            self.context_model,
            result_type=EventContextResult,
            system_prompt=EVENT_CONTEXT_PROMPT
        )
        
        self.comparison_agent = Agent(
            self.comparison_model,
            result_type=ComparativeAnalysisResult,
            system_prompt=COMPARATIVE_ANALYSIS_PROMPT
        )
        
        self.impact_agent = Agent(
            self.impact_model,
            result_type=UserImpactResult,
            system_prompt=USER_IMPACT_PROMPT
        )
    
    async def analyze_causality(self, request: CausalAnalysisRequest) -> CausalAnalysisResult:
        """Analyze what events might have caused a specific event."""
        # Get relevant logs before the event
        logs = self.duckdb_conn.execute(f"""
            SELECT * FROM log_entries 
            WHERE timestamp <= (
                SELECT timestamp FROM log_entries 
                WHERE message LIKE '%{request.event}%' 
                ORDER BY timestamp DESC LIMIT 1
            )
            AND timestamp >= (
                SELECT timestamp FROM log_entries 
                WHERE message LIKE '%{request.event}%' 
                ORDER BY timestamp DESC LIMIT 1
            ) - INTERVAL '{request.time_window}'
        """).fetchdf()
        
        # Use the causal agent to analyze the logs
        return await self.causal_agent.run(
            f"Analyze what might have caused the event: {request.event}",
            context={"logs": logs.to_dict()}
        )
    
    async def analyze_user_impact(self, request: UserImpactRequest) -> UserImpactResult:
        """Analyze how many users were affected by an event."""
        # Get logs related to the event
        logs = self.duckdb_conn.execute(f"""
            SELECT * FROM log_entries 
            WHERE message LIKE '%{request.event}%'
        """).fetchdf()
        
        # Use the impact agent to analyze user impact
        return await self.impact_agent.run(
            f"Analyze user impact for event: {request.event}",
            context={"logs": logs.to_dict(), "user_field": request.user_field}
        )
    
    async def analyze_service_dependencies(self, request: ServiceDependencyRequest) -> ServiceDependencyResult:
        """Analyze dependencies and interactions between services."""
        # Get service interaction logs
        logs = self.duckdb_conn.execute(f"""
            SELECT * FROM log_entries 
            WHERE message LIKE '%{request.service}%'
            AND timestamp >= NOW() - INTERVAL '{request.time_window}'
        """).fetchdf()
        
        # Use the dependency agent to analyze service interactions
        return await self.dependency_agent.run(
            f"Analyze service dependencies for: {request.service}",
            context={"logs": logs.to_dict()}
        )
    
    async def get_event_context(self, request: EventContextRequest) -> EventContextResult:
        """Get full context around a specific event."""
        # Get logs around the event
        logs = self.duckdb_conn.execute(f"""
            SELECT * FROM log_entries 
            WHERE timestamp BETWEEN 
                (SELECT timestamp FROM log_entries WHERE id = '{request.event_id}') - INTERVAL '{request.context_window}'
                AND 
                (SELECT timestamp FROM log_entries WHERE id = '{request.event_id}') + INTERVAL '{request.context_window}'
        """).fetchdf()
        
        # Use the context agent to analyze the surrounding events
        return await self.context_agent.run(
            f"Analyze context for event: {request.event_id}",
            context={"logs": logs.to_dict()}
        )
    
    async def compare_events(self, request: ComparativeAnalysisRequest) -> ComparativeAnalysisResult:
        """Compare two events or time periods."""
        # Get logs for both events
        logs1 = self.duckdb_conn.execute(f"""
            SELECT * FROM log_entries 
            WHERE message LIKE '%{request.event1}%'
        """).fetchdf()
        
        logs2 = self.duckdb_conn.execute(f"""
            SELECT * FROM log_entries 
            WHERE message LIKE '%{request.event2}%'
        """).fetchdf()
        
        # Use the comparison agent to analyze differences
        return await self.comparison_agent.run(
            f"Compare events: {request.event1} vs {request.event2}",
            context={
                "logs1": logs1.to_dict(),
                "logs2": logs2.to_dict(),
                "metrics": request.metrics
            }
        ) 