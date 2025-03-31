'use client'; // Add this directive for using hooks

import React, { useState } from 'react';
import ChatInterface from '@/components/ChatInterface';
import AnalysisView from '@/components/AnalysisView';

const API_URL = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:8000';

// --- Main Page Component ---
export default function Home() {
  const [activeTab, setActiveTab] = useState<'Analysis' | 'Log Explorer'>('Analysis');

  const handleFileUpload = async (file: File) => {
    const formData = new FormData();
    formData.append('file', file);

    try {
      const response = await fetch(`${API_URL}/api/upload-log-file`, {
        method: 'POST',
        body: formData,
      });

      if (!response.ok) {
        throw new Error(`Upload failed: ${response.statusText}`);
      }

      const result = await response.json();
      console.log('File uploaded successfully:', result);
      
      // Switch to Log Explorer tab after successful upload
      setActiveTab('Log Explorer');
    } catch (error) {
      console.error('Error uploading file:', error);
      alert('Failed to upload file. Please try again.');
    }
  };

  return (
    <main className="min-h-screen bg-white">
      {/* Header */}
      <header className="border-b">
        <div className="max-w-screen-xl mx-auto px-4 py-4 flex items-center justify-between">
          <h1 className="text-xl font-semibold">New Analysis</h1>
          <div className="flex items-center space-x-4">
            <button 
              onClick={() => setActiveTab('Analysis')}
              className={`px-4 py-2 ${activeTab === 'Analysis' ? 'text-gray-900' : 'text-gray-500'}`}
            >
              <div className="flex items-center space-x-2">
                <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5" viewBox="0 0 20 20" fill="currentColor">
                  <path d="M2 10a8 8 0 018-8v8h8a8 8 0 11-16 0z" />
                  <path d="M12 2.252A8.014 8.014 0 0117.748 8H12V2.252z" />
                </svg>
                <span>Analysis</span>
              </div>
            </button>
            <button 
              onClick={() => setActiveTab('Log Explorer')}
              className={`px-4 py-2 ${activeTab === 'Log Explorer' ? 'text-gray-900' : 'text-gray-500'}`}
            >
              <div className="flex items-center space-x-2">
                <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5" viewBox="0 0 20 20" fill="currentColor">
                  <path fillRule="evenodd" d="M4 4a2 2 0 012-2h8a2 2 0 012 2v12a2 2 0 01-2 2H6a2 2 0 01-2-2V4zm2 0h8v12H6V4z" clipRule="evenodd" />
                </svg>
                <span>Log Explorer</span>
              </div>
            </button>
          </div>
        </div>
      </header>

      {/* Main Content */}
      <div className="max-w-screen-xl mx-auto px-4 py-8">
        {activeTab === 'Analysis' ? (
          <AnalysisView onFileUpload={handleFileUpload} />
        ) : (
          <div className="h-[calc(100vh-8rem)]">
            <ChatInterface />
          </div>
        )}
      </div>
    </main>
  );
}
