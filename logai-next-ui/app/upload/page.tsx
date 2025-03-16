'use client';

import { useState, useCallback, useEffect } from 'react';
import { useDropzone } from 'react-dropzone';
import { useRouter } from 'next/navigation';
import Navigation from '../../components/Navigation';
import UploadProgressBar from '../../components/UploadProgressBar';
import { FaUpload, FaSpinner } from 'react-icons/fa';
import { toast } from 'react-toastify';
import { uploadFile, subscribeToUploadProgress, UploadProgressEvent } from '../../lib/api';

// Interface for uploaded file data
interface UploadedFileData {
  path: string;
  filename: string;
  original_name: string;
  size: number;
  parser_id: string;
}

export default function UploadPage() {
  const router = useRouter();
  const [file, setFile] = useState<File | null>(null);
  const [uploading, setUploading] = useState(false);
  const [uploadSuccess, setUploadSuccess] = useState(false);
  const [parserId, setParserId] = useState('drain');
  const [uploadedFilePath, setUploadedFilePath] = useState('');
  const [uploadId, setUploadId] = useState<string | null>(null);
  const [progress, setProgress] = useState<UploadProgressEvent | null>(null);

  // Cleanup function for the SSE connection
  const [closeEventSource, setCloseEventSource] = useState<(() => void) | null>(null);

  // Create a subscription to upload progress events
  useEffect(() => {
    if (uploadId) {
      const close = subscribeToUploadProgress(
        uploadId,
        (progressData) => {
          setProgress(progressData);
          
          // If upload is complete, set success state and prepare for redirect
          if (progressData.status === 'complete') {
            setUploading(false);
            setUploadSuccess(true);
            toast.success('File uploaded successfully!');
            
            // Automatically redirect to analysis page after success
            setTimeout(() => {
              router.push('/analysis');
            }, 1500);
          }
          
          // If there's an error, show error toast
          if (progressData.status === 'error') {
            setUploading(false);
            toast.error(progressData.message || 'Upload failed');
          }
        },
        (error) => {
          console.error('SSE Error:', error);
          toast.error('Connection to server lost. Please try again.');
          setUploading(false);
        }
      );
      
      setCloseEventSource(() => close);
      
      // Cleanup subscription when component unmounts or uploadId changes
      return () => {
        close();
      };
    }
  }, [uploadId, router]);

  const onDrop = useCallback((acceptedFiles: File[]) => {
    if (acceptedFiles.length > 0) {
      setFile(acceptedFiles[0]);
      setUploadSuccess(false);
      setProgress(null);
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
    setProgress(null);
    
    try {
      // Use the API client to upload the file
      const data = await uploadFile(file, parserId);
      
      // Store the file path for later use
      setUploadedFilePath(data.path);
      
      // Store upload ID for SSE subscription
      setUploadId(data.upload_id);
      
      // Store upload information in sessionStorage for use in the analysis page
      sessionStorage.setItem('uploadedFile', JSON.stringify({
        path: data.path,
        filename: data.filename,
        original_name: data.original_name,
        size: data.size,
        parser_id: parserId
      }));
      
    } catch (error) {
      console.error('Upload error:', error);
      toast.error(error instanceof Error ? error.message : 'Failed to upload file. Please try again.');
      setUploading(false);
    }
  };

  // Clean up event source when navigating away
  useEffect(() => {
    return () => {
      if (closeEventSource) {
        closeEventSource();
      }
    };
  }, [closeEventSource]);

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
                disabled={uploading}
              >
                <option value="drain">DRAIN Parser</option>
                <option value="csv">CSV Parser</option>
                <option value="json">JSON Parser</option>
                <option value="regex">Regex Parser</option>
              </select>
            </div>
            
            {!uploading && !uploadSuccess && (
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
            )}
            
            {file && !uploading && !uploadSuccess && (
              <div className="mt-6 p-4 bg-gray-50 rounded-lg">
                <p className="font-medium">Selected file:</p>
                <p className="text-sm text-gray-600">{file.name} ({(file.size / 1024).toFixed(2)} KB)</p>
              </div>
            )}
            
            {progress && (
              <UploadProgressBar 
                progress={progress.progress}
                status={progress.status}
                message={progress.message}
              />
            )}
            
            <div className="mt-6 flex justify-end">
              {!uploading && !uploadSuccess && (
                <button
                  onClick={handleUpload}
                  disabled={!file || uploading}
                  className={`btn-primary flex items-center ${(!file || uploading) ? 'opacity-50 cursor-not-allowed' : ''}`}
                >
                  Upload File
                </button>
              )}
              
              {uploading && !progress && (
                <button
                  disabled
                  className="btn-primary flex items-center opacity-50 cursor-not-allowed"
                >
                  <FaSpinner className="animate-spin mr-2" />
                  Initializing...
                </button>
              )}
              
              {uploadSuccess && (
                <button
                  onClick={() => router.push('/analysis')}
                  className="btn-primary flex items-center"
                >
                  Continue to Analysis
                </button>
              )}
            </div>
          </div>
        </div>
      </div>
    </main>
  );
} 