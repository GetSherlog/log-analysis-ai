import Navigation from '../components/Navigation'
import Link from 'next/link'
import { FaUpload, FaSearch, FaChartLine, FaRegLightbulb } from 'react-icons/fa'

export default function Home() {
  return (
    <main className="min-h-screen">
      <Navigation />
      
      <div className="py-12 bg-white">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="lg:text-center">
            <h2 className="text-base text-primary-600 font-semibold tracking-wide uppercase">Sherlog</h2>
            <p className="mt-2 text-3xl leading-8 font-extrabold tracking-tight text-gray-900 sm:text-4xl">
              High-performance log analysis
            </p>
            <p className="mt-4 max-w-2xl text-xl text-gray-500 lg:mx-auto">
              Advanced log analytics with powerful parsing, feature extraction, and anomaly detection capabilities.
            </p>
          </div>

          <div className="mt-10">
            <div className="space-y-10 md:space-y-0 md:grid md:grid-cols-2 md:gap-x-8 md:gap-y-10">
              <Link href="/upload">
                <div className="card hover:border-primary-500 hover:border-2 cursor-pointer">
                  <div className="flex items-center mb-4">
                    <div className="flex-shrink-0">
                      <div className="flex items-center justify-center h-12 w-12 rounded-md bg-primary-500 text-white">
                        <FaUpload />
                      </div>
                    </div>
                    <h3 className="ml-4 text-lg leading-6 font-medium text-gray-900">Upload Logs</h3>
                  </div>
                  <p className="text-base text-gray-500">
                    Upload your log files for analysis. Supports various formats including CSV, JSON, and custom regex patterns.
                  </p>
                </div>
              </Link>

              <Link href="/analysis">
                <div className="card hover:border-primary-500 hover:border-2 cursor-pointer">
                  <div className="flex items-center mb-4">
                    <div className="flex-shrink-0">
                      <div className="flex items-center justify-center h-12 w-12 rounded-md bg-primary-500 text-white">
                        <FaSearch />
                      </div>
                    </div>
                    <h3 className="ml-4 text-lg leading-6 font-medium text-gray-900">Parse & Extract</h3>
                  </div>
                  <p className="text-base text-gray-500">
                    Efficiently parse logs and extract features using high-performance algorithms like DRAIN log parser.
                  </p>
                </div>
              </Link>

              <Link href="/analysis">
                <div className="card hover:border-primary-500 hover:border-2 cursor-pointer">
                  <div className="flex items-center mb-4">
                    <div className="flex-shrink-0">
                      <div className="flex items-center justify-center h-12 w-12 rounded-md bg-primary-500 text-white">
                        <FaChartLine />
                      </div>
                    </div>
                    <h3 className="ml-4 text-lg leading-6 font-medium text-gray-900">Anomaly Detection</h3>
                  </div>
                  <p className="text-base text-gray-500">
                    Detect anomalies in your log data using machine learning algorithms including One-Class SVM and DBSCAN.
                  </p>
                </div>
              </Link>

              <Link href="/analysis">
                <div className="card hover:border-primary-500 hover:border-2 cursor-pointer">
                  <div className="flex items-center mb-4">
                    <div className="flex-shrink-0">
                      <div className="flex items-center justify-center h-12 w-12 rounded-md bg-primary-500 text-white">
                        <FaRegLightbulb />
                      </div>
                    </div>
                    <h3 className="ml-4 text-lg leading-6 font-medium text-gray-900">Insights</h3>
                  </div>
                  <p className="text-base text-gray-500">
                    Gain valuable insights from your log data with visualizations and actionable intelligence.
                  </p>
                </div>
              </Link>
            </div>
          </div>
        </div>
      </div>
    </main>
  )
} 