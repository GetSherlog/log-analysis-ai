/** @type {import('next').NextConfig} */
const nextConfig = {
  reactStrictMode: true,
  swcMinify: true,
  output: 'standalone' // Optimize for Docker deployments
}

module.exports = nextConfig 