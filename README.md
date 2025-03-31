# LogAI: AI-Powered Log Analysis Tool

LogAI is a powerful log analysis tool that combines the efficiency of C++ for log parsing and template extraction with the flexibility of Python for AI-powered analysis. It provides a comprehensive suite of tools for log analysis, visualization, and insights generation.

## Features

### Core Capabilities
- **Efficient Log Parsing**: C++-based log parsing with template extraction
- **Template Management**: Automatic template extraction and storage in DuckDB and Qdrant
- **SQL-like Querying**: Direct SQL queries against log data using DuckDB
- **Rich Visualization**: Support for various visualization libraries (matplotlib, seaborn, plotly)
- **Advanced Analytics**: Statistical analysis, anomaly detection, and pattern recognition

### AI-Powered Analysis
- **Natural Language Interface**: Ask questions about your logs in plain English
- **Specialized Analysis Agents**:
  - Causal Analysis: Identify potential causes of events
  - User Impact Analysis: Assess how many users were affected
  - Service Dependency Analysis: Map service interactions
  - Event Context Analysis: Provide comprehensive event context
  - Comparative Analysis: Compare different events or time periods

### Type-Safe AI Integration
- **Pydantic Models**: Strong type enforcement for all inputs and outputs
- **Structured Responses**: Well-defined response types for each analysis type
- **Model Selection**: Automatic model selection based on task complexity:
  - GPT-4 for complex analysis tasks
  - Claude for medium complexity tasks
  - Gemini for simpler tasks

### Data Analysis Features
- **Time-based Analysis**: Filter and analyze logs by time ranges
- **Level-based Filtering**: Filter logs by severity levels
- **Pattern Detection**: Identify and count log patterns
- **Statistical Analysis**: Calculate various statistics about log data
- **Trend Analysis**: Identify trending patterns in logs

## Installation

```bash
# Clone the repository
git clone https://github.com/yourusername/logai-cpp.git
cd logai-cpp

# Install Python dependencies
pip install -r requirements.txt

# Build C++ components
mkdir build && cd build
cmake ..
make
```

## Usage

### Basic Usage

```python
from logai_agent import LogAIAgent

# Initialize the agent
agent = LogAIAgent(provider="openai", api_key="your-api-key")

# Load a log file
agent.initialize("path/to/your/logs.log")

# Ask questions about your logs
response = await agent.chat_query("What caused the service outage at 2pm?")
print(response.answer)
```

### Advanced Analysis

```python
# Use specialized agents for specific analysis
causal_analysis = await agent.specialized_agents.analyze_causality(
    event="service outage",
    time_window="1h"
)

# Perform comparative analysis
comparison = await agent.specialized_agents.compare_events(
    event1="deployment",
    event2="error spike",
    metrics=["error_rate", "response_time"]
)

# Get user impact analysis
impact = await agent.specialized_agents.analyze_user_impact(
    event="login failure",
    user_field="user_id"
)
```

### Data Visualization

```python
# Generate visualizations using the data analysis agent
analysis_result = await agent.data_analysis_agent.analyze_logs(
    "Show error rate trends over time with a line plot"
)

# Access generated visualizations
for figure in analysis_result.figures:
    # Display or save the figure
    pass
```

## Architecture

### Components
1. **C++ Core**
   - Log parsing and template extraction
   - Template embedding generation
   - Qdrant integration for similarity search

2. **Python Interface**
   - Natural language processing
   - Data analysis and visualization
   - AI model integration

3. **AI Agents**
   - Main analysis agent for general queries
   - Specialized agents for specific analysis types
   - Data analysis agent for visualization and statistics

### Data Flow
1. Logs are parsed by C++ components
2. Templates are extracted and stored
3. Data is loaded into DuckDB
4. AI agents analyze the data using available tools
5. Results are returned in type-safe formats

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the MIT License - see the LICENSE file for details. 