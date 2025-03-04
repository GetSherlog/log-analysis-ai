'use client';

import { useState } from 'react';
import Navigation from '../../components/Navigation';
import { FaChartLine, FaGear, FaRegLightbulb, FaDownload, FaTable } from 'react-icons/fa6';
import { Bar } from 'react-chartjs-2';
import { Chart as ChartJS, CategoryScale, LinearScale, BarElement, Title, Tooltip, Legend } from 'chart.js';

// Register Chart.js components
ChartJS.register(CategoryScale, LinearScale, BarElement, Title, Tooltip, Legend);

export default function AnalysisPage() {
  const [activeStep, setActiveStep] = useState(2); // Start with parsing since upload is done
  const [parseProgress, setParseProgress] = useState(100); // Completed
  const [featureExtractionProgress, setFeatureExtractionProgress] = useState(70);
  const [anomalyDetectionProgress, setAnomalyDetectionProgress] = useState(0);
  const [showTemplates, setShowTemplates] = useState(false);

  // Mock data for log templates
  const templates = [
    { id: 1, template: 'User [*] logged in successfully', count: 245 },
    { id: 2, template: 'Failed login attempt for user [*] from IP [*]', count: 18 },
    { id: 3, template: 'Database connection failed: [*]', count: 7 },
    { id: 4, template: 'API request completed in [*] ms', count: 326 },
    { id: 5, template: 'System shutdown initiated by [*]', count: 3 },
  ];

  // Chart data for log templates
  const chartData = {
    labels: templates.map(t => `Template ${t.id}`),
    datasets: [
      {
        label: 'Occurrence Count',
        data: templates.map(t => t.count),
        backgroundColor: 'rgba(14, 165, 233, 0.6)',
        borderColor: 'rgb(14, 165, 233)',
        borderWidth: 1,
      },
    ],
  };

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

  const handleRunFeatureExtraction = () => {
    // Simulate feature extraction progress
    setActiveStep(3);
    let progress = 0;
    const interval = setInterval(() => {
      progress += 5;
      setFeatureExtractionProgress(progress);
      if (progress >= 100) {
        clearInterval(interval);
      }
    }, 200);
  };

  const handleRunAnomalyDetection = () => {
    // Simulate anomaly detection progress
    setActiveStep(4);
    let progress = 0;
    const interval = setInterval(() => {
      progress += 5;
      setAnomalyDetectionProgress(progress);
      if (progress >= 100) {
        clearInterval(interval);
      }
    }, 200);
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
              
              <div className="flex items-center text-primary-600 relative">
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
            
            {featureExtractionProgress === 100 && (
              <div className="mt-6 flex justify-end">
                <button
                  onClick={handleRunAnomalyDetection}
                  className="btn-primary"
                >
                  Run Anomaly Detection
                </button>
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
            
            {anomalyDetectionProgress === 100 && (
              <div className="mt-6 flex justify-end">
                <button
                  className="btn-primary flex items-center"
                >
                  <FaDownload className="mr-2" />
                  Download Results
                </button>
              </div>
            )}
          </div>
        </div>
      </div>
    </main>
  );
} 