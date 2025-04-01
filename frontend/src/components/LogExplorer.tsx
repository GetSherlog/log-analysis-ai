import React from 'react';
import { Card } from "@/components/ui/card";
import { ScrollArea } from "@/components/ui/scroll-area";

export default function LogExplorer() {
  return (
    <Card className="flex flex-col h-full border-0 rounded-none">
      <div className="p-4 border-b">
        <h2 className="text-lg font-semibold">Log Explorer</h2>
      </div>
      <ScrollArea className="flex-1">
        <div className="p-4">
          <p className="text-muted-foreground text-center">
            Upload log files in the Analysis tab to view them here
          </p>
        </div>
      </ScrollArea>
    </Card>
  );
} 