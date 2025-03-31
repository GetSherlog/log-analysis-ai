from typing import List, Dict, Any, Optional
import numpy as np
from qdrant_client import QdrantClient
from qdrant_client.http import models
from qdrant_client.http.models import Distance, VectorParams
from pydantic import BaseModel
import os
from dotenv import load_dotenv

load_dotenv()

class VectorStore:
    def __init__(self, collection_name: str = "log_templates"):
        self.collection_name = collection_name
        self.client = QdrantClient(
            host=os.getenv("QDRANT_HOST", "localhost"),
            port=int(os.getenv("QDRANT_PORT", "6333"))
        )
        self._ensure_collection_exists()

    def _ensure_collection_exists(self):
        """Ensure the collection exists, create if it doesn't."""
        collections = self.client.get_collections().collections
        collection_names = [collection.name for collection in collections]
        
        if self.collection_name not in collection_names:
            self.client.create_collection(
                collection_name=self.collection_name,
                vectors_config=VectorParams(
                    size=1536,  # OpenAI embedding dimension
                    distance=Distance.COSINE
                )
            )

    def store_template(self, template: str, embedding: List[float], metadata: Optional[Dict[str, Any]] = None):
        """Store a template with its embedding and metadata."""
        if metadata is None:
            metadata = {}
        
        # Generate a unique ID based on template content
        template_id = hash(template)
        
        self.client.upsert(
            collection_name=self.collection_name,
            points=[
                models.PointStruct(
                    id=template_id,
                    vector=embedding,
                    payload={
                        "template": template,
                        **metadata
                    }
                )
            ]
        )
        return template_id

    def search_similar(self, query_embedding: List[float], limit: int = 5) -> List[Dict[str, Any]]:
        """Search for similar templates using cosine similarity."""
        search_result = self.client.search(
            collection_name=self.collection_name,
            query_vector=query_embedding,
            limit=limit
        )
        
        return [
            {
                "template": hit.payload["template"],
                "score": hit.score,
                "metadata": {k: v for k, v in hit.payload.items() if k != "template"}
            }
            for hit in search_result
        ]

    def delete_template(self, template_id: int):
        """Delete a template by its ID."""
        self.client.delete(
            collection_name=self.collection_name,
            points_selector=models.PointIdsList(
                points=[template_id]
            )
        )

    def get_all_templates(self, limit: int = 100) -> List[Dict[str, Any]]:
        """Retrieve all templates with their metadata."""
        scroll_result = self.client.scroll(
            collection_name=self.collection_name,
            limit=limit
        )
        
        return [
            {
                "id": point.id,
                "template": point.payload["template"],
                "metadata": {k: v for k, v in point.payload.items() if k != "template"}
            }
            for point in scroll_result[0]
        ]

    def update_template_metadata(self, template_id: int, metadata: Dict[str, Any]):
        """Update metadata for an existing template."""
        self.client.update_payload(
            collection_name=self.collection_name,
            points_selector=models.PointIdsList(points=[template_id]),
            payload=metadata
        ) 