'use client';

import { useState, useEffect } from 'react';
import { useRouter } from 'next/navigation';
import Navigation from '../../components/Navigation';
import { FaChartLine, FaGear, FaRegLightbulb, FaDownload, FaTable } from 'react-icons/fa6';
import { Bar } from 'react-chartjs-2';
import { Chart as ChartJS, CategoryScale, LinearScale, BarElement, Title, Tooltip, Legend, ChartData } from 'chart.js';
import { toast } from 'react-toastify';
import { parseFile, extractFeatures, detectAnomaliesOcSvm, FileParserResponse, FeatureExtractionResponse, OcSvmAnomalyDetectionResponse } from '../../lib/api';

// Register Chart.js components
ChartJS.register(CategoryScale, LinearScale, BarElement, Title, Tooltip, Legend);

// Interface for uploaded file data
interface UploadedFileData {
  path: string;
  filename: string;
  original_name: string;
  size: number;
  parser_id: string;
}

export default function AnalysisPage() {
  const router = useRouter();
  const [activeStep, setActiveStep] = useState(1); // Start with parsing
  const [parseProgress, setParseProgress] = useState(0);
  const [featureExtractionProgress, setFeatureExtractionProgress] = useState(0);
  const [anomalyDetectionProgress, setAnomalyDetectionProgress] = useState(0);
  const [showTemplates, setShowTemplates] = useState(false);
  const [uploadedFile, setUploadedFile] = useState<UploadedFileData | null>(null);
  
  // State for storing API results
  const [parsingResults, setParsingResults] = useState<FileParserResponse | null>(null);
  const [templates, setTemplates] = useState<Array<{id: number, template: string, count: number}>>([]);
  const [featureResults, setFeatureResults] = useState<FeatureExtractionResponse | null>(null);
  const [anomalyResults, setAnomalyResults] = useState<OcSvmAnomalyDetectionResponse | null>(null);
  
  // Chart data with proper typing
  const [chartData, setChartData] = useState<ChartData<'bar', number[], string>>({
    labels: [],
    datasets: [
      {
        label: 'Occurrence Count',
        data: [],
        backgroundColor: 'rgba(14, 165, 233, 0.6)',
        borderColor: 'rgb(14, 165, 233)',
        borderWidth: 1,
      },
    ],
  });

  const chartOptions = {
    responsive: true,
    plugins: {
      legend: {
        position: 'top' as const,
      },
      title: {
        display: true,
        text: 'Log Template Distribution',
      },
    },
  };
  
  // Load uploaded file info from sessionStorage
  useEffect(() => {
    const storedData = sessionStorage.getItem('uploadedFile');
    if (storedData) {
      try {
        const fileData = JSON.parse(storedData) as UploadedFileData;
        setUploadedFile(fileData);
        // Start parsing automatically
        handleParseFile(fileData);
      } catch (error) {
        console.error('Error parsing stored file data:', error);
        toast.error('Error loading file information. Please upload your file again.');
        setTimeout(() => {
          router.push('/upload');
        }, 1500);
      }
    } else {
      toast.info('No uploaded file found. Please upload a file first.');
      setTimeout(() => {
        router.push('/upload');
      }, 1500);
    }
  }, [router]);

  const handleParseFile = async (fileData: UploadedFileData) => {
    setActiveStep(2);
    setParseProgress(10);
    
    try {
      // Call the file parser API
      const result = await parseFile({
        file_path: fileData.path,
        max_lines: 1000 // Limit to 1000 lines for the demo
      });
      
      setParseProgress(70);
      setParsingResults(result);
      
      // Use templates from the API response
      setTemplates(result.templates);
      setChartData({
        labels: result.templates.map(t => `Template ${t.id}`),
        datasets: [
          {
            label: 'Occurrence Count',
            data: result.templates.map(t => t.count),
            backgroundColor: 'rgba(14, 165, 233, 0.6)',
            borderColor: 'rgb(14, 165, 233)',
            borderWidth: 1,
          },
        ],
      });
      
      setParseProgress(100);
    } catch (error) {
      console.error('Parsing error:', error);
      toast.error(error instanceof Error ? error.message : 'Failed to parse file. Please try again.');
      setParseProgress(0);
    }
  };

  const handleRunFeatureExtraction = async () => {
    if (!parsingResults) {
      toast.error('No parsing results available. Please parse the file first.');
      return;
    }
    
    setActiveStep(3);
    setFeatureExtractionProgress(10);
    
    try {
      // Use templates for feature extraction
      const logLines = parsingResults.templates.map(t => t.template);
      
      // Call the feature extraction API
      const result = await extractFeatures({
        logLines: logLines
      });
      
      setFeatureResults(result);
      setFeatureExtractionProgress(100);
    } catch (error) {
      console.error('Feature extraction error:', error);
      toast.error(error instanceof Error ? error.message : 'Failed to extract features. Please try again.');
      setFeatureExtractionProgress(0);
    }
  };

  const handleRunAnomalyDetection = async () => {
    if (!featureResults) {
      toast.error('No feature results available. Please extract features first.');
      return;
    }
    
    setActiveStep(4);
    setAnomalyDetectionProgress(10);
    
    try {
      // Call the anomaly detection API
      const result = await detectAnomaliesOcSvm({
        featureVectors: featureResults.features,
        kernel: 'rbf',
        nu: 0.1
      });
      
      setAnomalyResults(result);
      setAnomalyDetectionProgress(100);
    } catch (error) {
      console.error('Anomaly detection error:', error);
      toast.error(error instanceof Error ? error.message : 'Failed to detect anomalies. Please try again.');
      setAnomalyDetectionProgress(0);
    }
  };
  
  const handleDownloadResults = () => {
    if (!anomalyResults) {
      toast.error('No anomaly detection results available');
      return;
    }
    
    // Create a JSON blob with the results
    const dataStr = JSON.stringify({
      parsing: parsingResults,
      features: featureResults,
      anomalies: anomalyResults
    }, null, 2);
    
    const blob = new Blob([dataStr], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    
    // Create download link and trigger download
    const a = document.createElement('a');
    a.href = url;
    a.download = 'sherlog-analysis-results.json';
    document.body.appendChild(a);
    a.click();
    
    // Clean up
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  };

  return (
    <main className="min-h-screen">
      <Navigation />
      
      <div className="py-12">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="mb-10">
            <h2 className="text-3xl font-extrabold text-gray-900">
              Log Analysis
            </h2>
            <p className="mt-2 text-lg text-gray-500">
              Process your logs through parsing, feature extraction, and anomaly detection.
            </p>
          </div>

          {/* Progress Steps */}
          <div className="mb-10">
            <div className="flex items-center w-full mb-6">
              <div className="flex items-center text-primary-600 relative">
                <div className="rounded-full h-12 w-12 flex items-center justify-center bg-primary-600 text-white">
                  1
                </div>
                <div className="absolute top-0 -ml-10 text-center mt-14 w-32 text-sm font-medium">
                  Upload
                </div>
              </div>
              <div className="flex-auto border-t-2 border-primary-600"></div>
              
              <div className={`flex items-center relative ${activeStep >= 2 ? 'text-primary-600' : 'text-gray-500'}`}>
                <div className={`rounded-full h-12 w-12 flex items-center justify-center ${activeStep >= 2 ? 'bg-primary-600 text-white' : 'bg-gray-200 text-gray-600'}`}>
                  2
                </div>
                <div className="absolute top-0 -ml-10 text-center mt-14 w-32 text-sm font-medium">
                  Log Parsing
                </div>
              </div>
              <div className={`flex-auto border-t-2 ${activeStep >= 3 ? 'border-primary-600' : 'border-gray-300'}`}></div>
              
              <div className={`flex items-center relative ${activeStep >= 3 ? 'text-primary-600' : 'text-gray-500'}`}>
                <div className={`rounded-full h-12 w-12 flex items-center justify-center ${activeStep >= 3 ? 'bg-primary-600 text-white' : 'bg-gray-200 text-gray-600'}`}>
                  3
                </div>
                <div className="absolute top-0 -ml-10 text-center mt-14 w-32 text-sm font-medium">
                  Feature Extraction
                </div>
              </div>
              <div className={`flex-auto border-t-2 ${activeStep >= 4 ? 'border-primary-600' : 'border-gray-300'}`}></div>
              
              <div className={`flex items-center relative ${activeStep >= 4 ? 'text-primary-600' : 'text-gray-500'}`}>
                <div className={`rounded-full h-12 w-12 flex items-center justify-center ${activeStep >= 4 ? 'bg-primary-600 text-white' : 'bg-gray-200 text-gray-600'}`}>
                  4
                </div>
                <div className="absolute top-0 -ml-10 text-center mt-14 w-32 text-sm font-medium">
                  Anomaly Detection
                </div>
              </div>
              <div className={`flex-auto border-t-2 ${activeStep >= 5 ? 'border-primary-600' : 'border-gray-300'}`}></div>
              
              <div className={`flex items-center relative ${activeStep >= 5 ? 'text-primary-600' : 'text-gray-500'}`}>
                <div className={`rounded-full h-12 w-12 flex items-center justify-center ${activeStep >= 5 ? 'bg-primary-600 text-white' : 'bg-gray-200 text-gray-600'}`}>
                  5
                </div>
                <div className="absolute top-0 -ml-10 text-center mt-14 w-32 text-sm font-medium">
                  Results
                </div>
              </div>
            </div>
          </div>

          {/* Log Parsing Section */}
          <div className="mb-8 card">
            <div className="flex justify-between items-center mb-4">
              <h3 className="text-xl font-bold flex items-center">
                <FaGear className="mr-2" /> Log Parsing
              </h3>
              <div className="text-sm text-gray-500">
                {parseProgress}% Complete
              </div>
            </div>
            
            <div className="w-full bg-gray-200 rounded-full h-2.5 mb-4">
              <div 
                className="bg-primary-600 h-2.5 rounded-full" 
                style={{ width: `${parseProgress}%` }}
              ></div>
            </div>
            
            {uploadedFile && (
              <div className="mb-4 p-4 bg-gray-50 rounded-lg">
                <p className="font-medium">Processing file:</p>
                <p className="text-sm text-gray-600">{uploadedFile.original_name} ({(uploadedFile.size / 1024).toFixed(2)} KB)</p>
              </div>
            )}
            
            {parseProgress === 100 && (
              <div className="mt-4">
                <div className="flex justify-between items-center mb-4">
                  <h4 className="font-medium">Parsing Results</h4>
                  <button 
                    onClick={() => setShowTemplates(!showTemplates)}
                    className="text-primary-600 text-sm flex items-center"
                  >
                    <FaTable className="mr-1" />
                    {showTemplates ? 'Hide Templates' : 'Show Templates'}
                  </button>
                </div>
                
                {showTemplates && (
                  <div className="overflow-x-auto">
                    <table className="min-w-full divide-y divide-gray-200">
                      <thead className="bg-gray-50">
                        <tr>
                          <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                            ID
                          </th>
                          <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                            Template
                          </th>
                          <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                            Occurrences
                          </th>
                        </tr>
                      </thead>
                      <tbody className="bg-white divide-y divide-gray-200">
                        {templates.map((template) => (
                          <tr key={template.id}>
                            <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                              {template.id}
                            </td>
                            <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                              {template.template}
                            </td>
                            <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                              {template.count}
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                )}
                
                <div className="mt-6">
                  <Bar data={chartData} options={chartOptions} height={100} />
                </div>
                
                <div className="mt-6 flex justify-end">
                  <button
                    onClick={handleRunFeatureExtraction}
                    className="btn-primary"
                  >
                    Run Feature Extraction
                  </button>
                </div>
              </div>
            )}
          </div>

          {/* Feature Extraction Section */}
          <div className={`mb-8 card ${activeStep < 3 ? 'opacity-50' : ''}`}>
            <div className="flex justify-between items-center mb-4">
              <h3 className="text-xl font-bold flex items-center">
                <FaRegLightbulb className="mr-2" /> Feature Extraction
              </h3>
              <div className="text-sm text-gray-500">
                {featureExtractionProgress}% Complete
              </div>
            </div>
            
            <div className="w-full bg-gray-200 rounded-full h-2.5 mb-4">
              <div 
                className="bg-primary-600 h-2.5 rounded-full" 
                style={{ width: `${featureExtractionProgress}%` }}
              ></div>
            </div>
            
            {featureExtractionProgress === 100 && featureResults && (
              <div className="mt-4">
                <div className="p-4 bg-gray-50 rounded-lg">
                  <p className="font-medium">Feature Extraction Results:</p>
                  <p className="text-sm text-gray-600">Total features: {featureResults.totalFeatures}</p>
                  <p className="text-sm text-gray-600">Feature dimension: {featureResults.featureDimension}</p>
                </div>
                
                <div className="mt-6 flex justify-end">
                  <button
                    onClick={handleRunAnomalyDetection}
                    className="btn-primary"
                  >
                    Run Anomaly Detection
                  </button>
                </div>
              </div>
            )}
          </div>

          {/* Anomaly Detection Section */}
          <div className={`mb-8 card ${activeStep < 4 ? 'opacity-50' : ''}`}>
            <div className="flex justify-between items-center mb-4">
              <h3 className="text-xl font-bold flex items-center">
                <FaChartLine className="mr-2" /> Anomaly Detection
              </h3>
              <div className="text-sm text-gray-500">
                {anomalyDetectionProgress}% Complete
              </div>
            </div>
            
            <div className="w-full bg-gray-200 rounded-full h-2.5 mb-4">
              <div 
                className="bg-primary-600 h-2.5 rounded-full" 
                style={{ width: `${anomalyDetectionProgress}%` }}
              ></div>
            </div>
            
            {anomalyDetectionProgress === 100 && anomalyResults && (
              <div className="mt-4">
                <div className="p-4 bg-gray-50 rounded-lg">
                  <p className="font-medium">Anomaly Detection Results:</p>
                  <p className="text-sm text-gray-600">Total samples: {anomalyResults.totalSamples}</p>
                  <p className="text-sm text-gray-600">Anomalies found: {anomalyResults.anomalyCount}</p>
                  <p className="text-sm text-gray-600">Anomaly percentage: {((anomalyResults.anomalyCount / anomalyResults.totalSamples) * 100).toFixed(2)}%</p>
                </div>
                
                <div className="mt-6 flex justify-end">
                  <button
                    onClick={handleDownloadResults}
                    className="btn-primary flex items-center"
                  >
                    <FaDownload className="mr-2" />
                    Download Results
                  </button>
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </main>
  );
} 