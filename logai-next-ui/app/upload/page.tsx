'use client';

import { useState, useCallback } from 'react';
import { useDropzone } from 'react-dropzone';
import Navigation from '../../components/Navigation';
import { FaUpload, FaSpinner } from 'react-icons/fa';
import { toast } from 'react-toastify';

export default function UploadPage() {
  const [file, setFile] = useState<File | null>(null);
  const [uploading, setUploading] = useState(false);
  const [uploadSuccess, setUploadSuccess] = useState(false);
  const [parserId, setParserId] = useState('drain');

  const onDrop = useCallback((acceptedFiles: File[]) => {
    if (acceptedFiles.length > 0) {
      setFile(acceptedFiles[0]);
      setUploadSuccess(false);
    }
  }, []);

  const { getRootProps, getInputProps, isDragActive } = useDropzone({ 
    onDrop,
    accept: {
      'text/plain': ['.log', '.txt'],
      'text/csv': ['.csv'],
      'application/json': ['.json'],
    },
    maxFiles: 1
  });

  const handleUpload = async () => {
    if (!file) {
      toast.error('Please select a file to upload');
      return;
    }

    setUploading(true);
    
    try {
      const formData = new FormData();
      formData.append('file', file);
      formData.append('parser_id', parserId);
      
      // In a real implementation, this would be a fetch to your API
      await new Promise(resolve => setTimeout(resolve, 2000)); // Simulating API call
      
      // Mock successful upload
      setUploadSuccess(true);
      toast.success('File uploaded successfully!');
      
      // Redirect to parsing page would happen here in a real implementation
      // router.push('/analysis');
    } catch (error) {
      console.error('Upload error:', error);
      toast.error('Failed to upload file. Please try again.');
    } finally {
      setUploading(false);
    }
  };

  return (
    <main className="min-h-screen">
      <Navigation />
      
      <div className="py-12">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="lg:text-center mb-10">
            <h2 className="text-base text-primary-600 font-semibold tracking-wide uppercase">Step 1</h2>
            <p className="mt-2 text-3xl leading-8 font-extrabold tracking-tight text-gray-900 sm:text-4xl">
              Upload Log Files
            </p>
            <p className="mt-4 max-w-2xl text-xl text-gray-500 lg:mx-auto">
              Upload your log files for analysis. Supported formats include CSV, JSON, and text logs.
            </p>
          </div>

          <div className="bg-white shadow rounded-lg p-6">
            <div className="mb-6">
              <label className="block text-sm font-medium text-gray-700 mb-2">
                Select Parser Type
              </label>
              <select
                value={parserId}
                onChange={(e) => setParserId(e.target.value)}
                className="input"
              >
                <option value="drain">DRAIN Parser</option>
                <option value="csv">CSV Parser</option>
                <option value="json">JSON Parser</option>
                <option value="regex">Regex Parser</option>
              </select>
            </div>
            
            <div 
              {...getRootProps()} 
              className={`border-2 border-dashed rounded-lg p-10 text-center cursor-pointer transition-colors ${
                isDragActive ? 'border-primary-500 bg-primary-50' : 'border-gray-300 hover:border-primary-400'
              }`}
            >
              <input {...getInputProps()} />
              
              <div className="flex flex-col items-center justify-center space-y-4">
                <FaUpload className="text-4xl text-gray-400" />
                
                {isDragActive ? (
                  <p className="text-lg font-medium text-primary-500">Drop the files here...</p>
                ) : (
                  <p className="text-lg font-medium text-gray-500">
                    Drag & drop log files here, or click to select files
                  </p>
                )}
                
                <p className="text-sm text-gray-400">
                  Supports .log, .txt, .csv, and .json files
                </p>
              </div>
            </div>
            
            {file && (
              <div className="mt-6 p-4 bg-gray-50 rounded-lg">
                <p className="font-medium">Selected file:</p>
                <p className="text-sm text-gray-600">{file.name} ({(file.size / 1024).toFixed(2)} KB)</p>
              </div>
            )}
            
            <div className="mt-6 flex justify-end">
              <button
                onClick={handleUpload}
                disabled={!file || uploading}
                className={`btn-primary flex items-center ${(!file || uploading) ? 'opacity-50 cursor-not-allowed' : ''}`}
              >
                {uploading ? (
                  <>
                    <FaSpinner className="animate-spin mr-2" />
                    Uploading...
                  </>
                ) : uploadSuccess ? (
                  'Continue to Analysis'
                ) : (
                  'Upload File'
                )}
              </button>
            </div>
          </div>
        </div>
      </div>
    </main>
  );
} 