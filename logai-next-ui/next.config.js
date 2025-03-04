/** @type {import('next').NextConfig} */
const nextConfig = {
  reactStrictMode: true,
  swcMinify: true,
  output: 'standalone', // Optimize for Docker deployments
  async rewrites() {
    return [
      {
        source: '/api/:path*',
        // Use environment variable or default to localhost for development
        destination: process.env.NEXT_PUBLIC_API_URL 
          ? `${process.env.NEXT_PUBLIC_API_URL}/:path*` 
          : 'http://localhost:8080/:path*'
      }
    ]
  }
}

module.exports = nextConfig 