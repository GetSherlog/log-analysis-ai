# LogAI Next.js UI

This is a modern web interface for the LogAI-CPP high-performance log analysis library, built with Next.js, TypeScript, and Tailwind CSS.

## Features

- Modern, responsive UI using Tailwind CSS
- Complete workflow from log upload to anomaly detection
- Interactive visualizations of log data
- Real-time processing feedback
- Docker-ready for easy deployment

## Project Structure

```
logai-next-ui/
├── app/                  # Next.js app router
│   ├── page.tsx          # Home page
│   ├── upload/           # File upload page
│   ├── analysis/         # Analysis workflow pages
│   └── layout.tsx        # Root layout
├── components/           # Reusable React components
├── lib/                  # Utility functions and hooks
├── public/               # Static assets
└── styles/               # Global styles
```

## Prerequisites

- Node.js 16+ (for local development)
- Docker and Docker Compose (for containerized deployment)

## Development

To run the development server:

```bash
npm install
npm run dev
```

Open [http://localhost:3000](http://localhost:3000) with your browser to see the result.

## Docker Deployment

The entire application (Next.js frontend + LogAI backend) can be deployed using Docker Compose:

```bash
# From the root directory of the project
chmod +x start-next-ui.sh
./start-next-ui.sh
```

This will:
1. Build both the backend and frontend Docker images
2. Start the services with proper networking
3. Mount volumes for logs and uploads
4. Provide health checks

## Workflow

The application follows a 5-step workflow:

1. **Upload File**: Upload log files for analysis
2. **Log Parsing**: Parse logs using DRAIN or other parsers
3. **Feature Extraction**: Extract meaningful features from logs
4. **Anomaly Detection**: Detect anomalies in the log data
5. **Results Display**: View and analyze the results

## API Integration

The Next.js frontend communicates with the LogAI-CPP backend through a RESTful API. Key endpoints:

- `/api/upload`: Upload log files
- `/api/parse`: Parse logs with the selected parser
- `/api/extract`: Extract features from parsed logs
- `/api/detect`: Run anomaly detection algorithms
- `/api/results`: Retrieve analysis results

## License

Same as the main LogAI-CPP project 