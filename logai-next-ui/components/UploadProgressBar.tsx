import React from 'react';

interface UploadProgressBarProps {
  progress: number;
  status: 'initializing' | 'uploading' | 'processing' | 'complete' | 'error';
  message: string;
}

const UploadProgressBar: React.FC<UploadProgressBarProps> = ({ progress, status, message }) => {
  // Determine color based on status
  const getColorClass = () => {
    switch (status) {
      case 'initializing':
        return 'bg-gray-400';
      case 'uploading':
        return 'bg-blue-500';
      case 'processing':
        return 'bg-yellow-500';
      case 'complete':
        return 'bg-green-500';
      case 'error':
        return 'bg-red-500';
      default:
        return 'bg-blue-500';
    }
  };

  // Cap progress at 100%
  const clampedProgress = Math.min(Math.max(0, progress), 100);
  
  return (
    <div className="w-full mt-6">
      <div className="flex justify-between mb-1">
        <span className="text-base font-medium text-gray-700">
          {status === 'uploading' ? 'Uploading' : 
           status === 'processing' ? 'Processing' : 
           status === 'complete' ? 'Complete' : 
           status === 'error' ? 'Error' : 
           'Initializing'
          }
        </span>
        <span className="text-sm font-medium text-gray-600">{clampedProgress}%</span>
      </div>
      <div className="w-full bg-gray-200 rounded-full h-2.5">
        <div 
          className={`h-2.5 rounded-full transition-all duration-300 ${getColorClass()}`} 
          style={{ width: `${clampedProgress}%` }}
        ></div>
      </div>
      <p className="text-sm text-gray-500 mt-1">{message}</p>
    </div>
  );
};

export default UploadProgressBar; 