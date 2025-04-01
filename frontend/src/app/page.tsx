'use client'; // Add this directive for using hooks

import React, { useState } from 'react';
import ChatInterface from '@/components/ChatInterface';
import LogExplorer from '@/components/LogExplorer';

export default function Home() {
  const [activeTab, setActiveTab] = useState<'analysis' | 'explorer'>('analysis');

  return (
    <main className="flex min-h-screen flex-col">
      <header className="border-b">
        <div className="container mx-auto px-4 py-4">
          <div className="flex space-x-4">
            <button
              onClick={() => setActiveTab('analysis')}
              className={`px-4 py-2 rounded-md ${
                activeTab === 'analysis'
                  ? 'bg-primary text-primary-foreground'
                  : 'text-muted-foreground hover:text-foreground'
              }`}
            >
              Analysis
            </button>
            <button
              onClick={() => setActiveTab('explorer')}
              className={`px-4 py-2 rounded-md ${
                activeTab === 'explorer'
                  ? 'bg-primary text-primary-foreground'
                  : 'text-muted-foreground hover:text-foreground'
              }`}
            >
              Log Explorer
            </button>
          </div>
        </div>
      </header>

      <div className="flex-1 container mx-auto px-4 py-4">
        {activeTab === 'analysis' ? (
          <ChatInterface />
        ) : (
          <LogExplorer />
        )}
      </div>
    </main>
  );
}
