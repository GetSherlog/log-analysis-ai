#!/usr/bin/env python3
import os
import json
import logging
from typing import List, Dict, Any, Optional, Literal
from pydantic import BaseModel, Field

from fastapi import FastAPI, HTTPException, Depends, UploadFile, File, Form, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, StreamingResponse

# Import our LogAI agent
from logai_agent import LogAIAgent, SearchResult, QueryResult, LogTemplate, TimeRange, CountResult

# Configure logging
logs_dir = os.path.join(os.path.dirname(__file__), '..', 'logs')
os.makedirs(logs_dir, exist_ok=True)
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler(os.path.join(logs_dir, 'server.log'), mode='a')
    ]
)
logger = logging.getLogger("logai-server")

# Create FastAPI app
app = FastAPI(
    title="LogAI API Server",
    description="API Server for LogAI - AI-powered log analysis",
    version="1.0.0"
)

# Add CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:3000", "*"],  # Allow frontend origin and all origins during development
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Global agent instance
logai_agent = None

# Request and response models
class InitializeRequest(BaseModel):
    log_file: str
    format: Optional[str] = None

class InitializeResponse(BaseModel):
    success: bool
    message: str

class SearchRequest(BaseModel):
    query: str
    limit: int = 10

class ExecuteQueryRequest(BaseModel):
    query: str

class TemplateRequest(BaseModel):
    template_id: int

class CountOccurrencesRequest(BaseModel):
    pattern: str
    group_by: Optional[str] = None

class FilterByTimeRequest(BaseModel):
    since: Optional[str] = None
    until: Optional[str] = None

class FilterByLevelRequest(BaseModel):
    levels: Optional[List[str]] = None
    exclude_levels: Optional[List[str]] = None

class SummarizeRequest(BaseModel):
    time_range: Optional[Dict[str, str]] = None

class TrendingPatternsRequest(BaseModel):
    time_window: str = "hour"

class LogAIAgentConfig(BaseModel):
    """Configuration for the LogAI agent."""
    provider: Literal["openai"] = "openai"
    api_key: Optional[str] = None
    model: Optional[str] = None

class AnalysisRequest(BaseModel):
    """Request for data analysis tasks."""
    task: str = Field(..., description="Description of the analysis task to perform")

class AnalysisResponse(BaseModel):
    """Response for data analysis tasks."""
    success: bool = Field(..., description="Whether the analysis was successful")
    analysis: str = Field(..., description="Textual analysis result")
    has_visualizations: bool = Field(..., description="Whether visualizations were generated")
    visualizations: List[str] = Field(default_factory=list, description="Base64-encoded visualizations")
    error: Optional[str] = Field(None, description="Error message if analysis failed")

class ChatRequest(BaseModel):
    query: str

class ChatResponse(BaseModel):
    response: str

class LogEntryResponse(BaseModel):
    id: int
    timestamp: str
    level: str
    message: str

# Dependency to check if agent is initialized
def get_initialized_agent():
    if logai_agent is None or not logai_agent.is_initialized:
        raise HTTPException(status_code=400, detail="LogAI Agent not initialized. Call /initialize first.")
    return logai_agent

# Routes
@app.post("/api/configure", response_model=dict)
async def configure_agent(config: LogAIAgentConfig):
    global logai_agent
    logger.info(f"Configuring LogAI agent with provider: {config.provider}, model: {config.model}")
    try:
        if not config.api_key:
            raise ValueError("OpenAI API key is required")
            
        logai_agent = LogAIAgent(
            provider=config.provider,
            api_key=config.api_key,
            model=config.model
        )
        logger.info("Agent configured successfully")
        return {"success": True, "message": "Agent configured successfully"}
    except Exception as e:
        logger.error(f"Failed to configure agent: {str(e)}", exc_info=True)
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/initialize", response_model=InitializeResponse)
async def initialize(request: InitializeRequest):
    global logai_agent
    logger.info(f"Initializing LogAI agent with log file: {request.log_file}, format: {request.format}")
    
    if logai_agent is None:
        # Use default configuration if not explicitly configured
        logger.info("Agent not configured, using default configuration (OpenAI)")
        logai_agent = LogAIAgent(provider="openai")
    
    try:
        success = logai_agent.initialize(request.log_file, request.format)
        if success:
            logger.info("LogAI Agent initialized successfully")
            return InitializeResponse(success=True, message="LogAI Agent initialized successfully")
        else:
            logger.warning(f"Failed to initialize LogAI Agent with file: {request.log_file}")
            return InitializeResponse(success=False, message="Failed to initialize LogAI Agent")
    except Exception as e:
        logger.error(f"Error initializing agent: {str(e)}", exc_info=True)
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/upload-log-file")
async def upload_log_file(file: UploadFile = File(...), format: Optional[str] = Form(None)):
    global logai_agent
    
    logger.info(f"Received file upload request: {file.filename}, format: {format}")
    
    if logai_agent is None:
        # Use default configuration if not explicitly configured
        logger.info("Agent not configured, using default configuration (OpenAI)")
        logai_agent = LogAIAgent(provider="openai")
    
    try:
        # Ensure the /tmp directory exists
        os.makedirs("/tmp", exist_ok=True)
        logger.info("Ensured /tmp directory exists")
        
        # Save the uploaded file
        file_location = f"/tmp/{file.filename}"
        logger.info(f"Saving uploaded file to {file_location}")
        
        try:
            contents = await file.read()
            logger.info(f"Read {len(contents)} bytes from uploaded file")
            
            with open(file_location, "wb") as f:
                f.write(contents)
                logger.info(f"Successfully wrote file to {file_location}")
        except Exception as file_error:
            logger.error(f"Error writing file: {str(file_error)}", exc_info=True)
            return JSONResponse(
                status_code=500,
                content={"success": False, "message": f"Error saving file: {str(file_error)}"}
            )
        
        logger.info(f"Successfully saved file, initializing agent...")
        # Initialize the agent with the uploaded file
        success = logai_agent.initialize(file_location, format)
        if success:
            logger.info(f"Successfully initialized agent with file {file_location}")
            return {"success": True, "message": "Log file uploaded and parsed successfully"}
        else:
            logger.warning(f"Failed to initialize agent with file {file_location}")
            return {"success": False, "message": "Failed to parse the uploaded log file"}
    except Exception as e:
        logger.error(f"Error processing file upload: {str(e)}", exc_info=True)
        # Return a proper JSON response instead of raising an exception
        return JSONResponse(
            status_code=500,
            content={"success": False, "message": f"Error processing file upload: {str(e)}"}
        )

@app.post("/api/search", response_model=List[Dict[str, Any]])
async def search_logs(request: SearchRequest, agent: LogAIAgent = Depends(get_initialized_agent)):
    try:
        results = agent.search_logs(request.query, request.limit)
        return results
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/execute-query", response_model=Dict[str, Any])
async def execute_query(request: ExecuteQueryRequest, agent: LogAIAgent = Depends(get_initialized_agent)):
    try:
        results = agent.execute_query(request.query)
        return results
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/template/{template_id}", response_model=Dict[str, Any])
async def get_template(template_id: str, agent: LogAIAgent = Depends(get_initialized_agent)):
    try:
        template = agent.get_template(template_id)
        if template is None:
            raise HTTPException(status_code=404, detail=f"Template with ID {template_id} not found")
        return template
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/time-range", response_model=Dict[str, str])
async def get_time_range(agent: LogAIAgent = Depends(get_initialized_agent)):
    try:
        time_range = agent.get_time_range()
        return time_range
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/count-occurrences", response_model=Dict[str, Any])
async def count_occurrences(request: CountOccurrencesRequest, agent: LogAIAgent = Depends(get_initialized_agent)):
    try:
        results = agent.count_occurrences(request.pattern, request.group_by)
        return results
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/summarize", response_model=Dict[str, Any])
async def summarize_logs(request: SummarizeRequest, agent: LogAIAgent = Depends(get_initialized_agent)):
    try:
        summary = agent.summarize_logs(request.time_range)
        return summary
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/filter-by-time", response_model=List[Dict[str, Any]])
async def filter_by_time(request: FilterByTimeRequest, agent: LogAIAgent = Depends(get_initialized_agent)):
    try:
        logs = agent.filter_by_time(request.since, request.until)
        return logs
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/filter-by-level", response_model=List[Dict[str, Any]])
async def filter_by_level(request: FilterByLevelRequest, agent: LogAIAgent = Depends(get_initialized_agent)):
    try:
        logs = agent.filter_by_level(request.levels, request.exclude_levels)
        return logs
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/statistics", response_model=Dict[str, Any])
async def calculate_statistics(agent: LogAIAgent = Depends(get_initialized_agent)):
    try:
        statistics = agent.calculate_statistics()
        return statistics
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/trending-patterns", response_model=List[Dict[str, Any]])
async def get_trending_patterns(request: TrendingPatternsRequest, agent: LogAIAgent = Depends(get_initialized_agent)):
    try:
        patterns = agent.get_trending_patterns(request.time_window)
        return patterns
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/analyze", response_model=AnalysisResponse)
async def analyze_logs(request: AnalysisRequest, agent: LogAIAgent = Depends(get_initialized_agent)):
    """Perform advanced log analysis with visualizations."""
    try:
        # Call the agent's analyze_logs method with the requested task
        result = agent.analyze_logs(request.task)
        
        if "error" in result and result["error"]:
            raise HTTPException(status_code=500, detail=result["error"])
        
        # Extract the visualizations if any
        visualizations = []
        if "full_result" in result and "figures" in result["full_result"]:
            visualizations = result["full_result"]["figures"]
        
        # Prepare the response
        return AnalysisResponse(
            success=result.get("success", False),
            analysis=result.get("analysis", "No analysis available"),
            has_visualizations=result.get("has_visualizations", False),
            visualizations=visualizations,
            error=result.get("error", None)
        )
    except HTTPException as http_exc:
        # Re-raise HTTP exceptions
        raise http_exc
    except Exception as e:
        print(f"Error in analyze_logs endpoint: {e}")
        raise HTTPException(status_code=500, detail=f"Error performing log analysis: {str(e)}")

@app.get("/health")
async def health_check():
    return {"status": "ok"}

@app.get("/api/logs", response_model=List[LogEntryResponse])
async def get_logs(agent: LogAIAgent = Depends(get_initialized_agent)):
    """Fetches recent log entries."""
    try:
        # Fetch the latest 100 logs from DuckDB
        query = "SELECT id, timestamp, level, message FROM log_entries ORDER BY id DESC LIMIT 100"
        query_result = agent.execute_query(query)

        if not query_result or 'rows' not in query_result or 'columns' not in query_result:
            raise HTTPException(status_code=500, detail="Failed to retrieve logs from database")

        # Map the results to the response model
        cols = query_result['columns']
        response_data = [
            LogEntryResponse(
                id=row[cols.index('id')],
                timestamp=str(row[cols.index('timestamp')]),
                level=str(row[cols.index('level')]),
                message=str(row[cols.index('message')])
            )
            for row in query_result['rows']
        ]
        return response_data

    except HTTPException as http_exc:
        # Re-raise HTTPExceptions to return proper status codes
        raise http_exc
    except Exception as e:
        print(f"Error fetching logs: {e}") # Add logging
        raise HTTPException(status_code=500, detail=f"Error fetching logs: {str(e)}")

@app.post("/api/chat", response_model=ChatResponse)
async def chat_endpoint(request: ChatRequest, agent: LogAIAgent = Depends(get_initialized_agent)):
    """Handles chat queries about the logs."""
    try:
        query_text = request.query
        logger.info(f"Received chat query: {query_text}")

        # Call the agent's chat method
        logger.info("Processing chat query with agent")
        response_text = await agent.chat_query(query_text)
        logger.info(f"Chat query processed successfully, response length: {len(response_text)}")

        return ChatResponse(response=response_text)

    except HTTPException as http_exc:
        # Re-raise HTTPExceptions (e.g., if agent not initialized)
        logger.warning(f"HTTP exception in chat endpoint: {http_exc.detail}")
        raise http_exc
    except Exception as e:
        logger.error(f"Error in chat endpoint: {str(e)}", exc_info=True)
        raise HTTPException(status_code=500, detail=f"Error processing chat query: {str(e)}")

if __name__ == "__main__":
    import uvicorn
    logger.info("Starting LogAI API Server on port 8000...")
    uvicorn.run("logai_server:app", host="0.0.0.0", port=8000, reload=True) 