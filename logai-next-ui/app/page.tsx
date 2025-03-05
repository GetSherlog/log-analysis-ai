import Navigation from '../components/Navigation'
import Link from 'next/link'
import { FaUpload, FaSearch, FaChartLine, FaRegLightbulb, FaLongArrowAltRight } from 'react-icons/fa'

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
            <h3 className="text-lg font-medium text-gray-900 mb-6">Our Sequential Log Analysis Workflow:</h3>
            
            <div className="relative">
              {/* Timeline connector */}
              <div className="hidden md:block absolute left-1/2 transform -translate-x-1/2 h-full w-1 bg-primary-200"></div>
              
              {/* Step 1 */}
              <Link href="/upload">
                <div className="relative mb-12">
                  <div className="flex md:flex-row flex-col items-center">
                    <div className="md:w-1/2 flex justify-end pr-8 pb-4 md:pb-0">
                      <div className="card hover:border-primary-500 hover:border-2 cursor-pointer md:max-w-md w-full">
                        <div className="flex items-center mb-4">
                          <div className="flex-shrink-0">
                            <div className="flex items-center justify-center h-12 w-12 rounded-md bg-primary-500 text-white">
                              <FaUpload />
                            </div>
                          </div>
                          <h3 className="ml-4 text-lg leading-6 font-medium text-gray-900">Step 1: Upload Logs</h3>
                        </div>
                        <p className="text-base text-gray-500">
                          Upload your log files for analysis. Supports various formats including CSV, JSON, and custom regex patterns.
                        </p>
                      </div>
                    </div>
                    
                    <div className="hidden md:flex absolute left-1/2 transform -translate-x-1/2 items-center justify-center h-10 w-10 rounded-full bg-primary-500 text-white font-bold">1</div>
                    
                    <div className="md:w-1/2 pl-8">
                      {/* Empty for visual balance */}
                    </div>
                  </div>
                  <div className="hidden md:block absolute right-1/2 bottom-0 transform translate-x-6 text-primary-600 text-3xl">
                    <FaLongArrowAltRight className="transform rotate-90" />
                  </div>
                </div>
              </Link>
              
              {/* Step 2 */}
              <Link href="/analysis">
                <div className="relative mb-12">
                  <div className="flex md:flex-row flex-col items-center">
                    <div className="md:w-1/2 pl-8 order-2 md:order-none pb-4 md:pb-0">
                      <div className="card hover:border-primary-500 hover:border-2 cursor-pointer md:max-w-md w-full">
                        <div className="flex items-center mb-4">
                          <div className="flex-shrink-0">
                            <div className="flex items-center justify-center h-12 w-12 rounded-md bg-primary-500 text-white">
                              <FaSearch />
                            </div>
                          </div>
                          <h3 className="ml-4 text-lg leading-6 font-medium text-gray-900">Step 2: Parse & Extract</h3>
                        </div>
                        <p className="text-base text-gray-500">
                          Efficiently parse logs and extract features using high-performance algorithms like DRAIN log parser.
                        </p>
                      </div>
                    </div>
                    
                    <div className="hidden md:flex absolute left-1/2 transform -translate-x-1/2 items-center justify-center h-10 w-10 rounded-full bg-primary-500 text-white font-bold">2</div>
                    
                    <div className="md:w-1/2 pr-8 order-1 md:order-none">
                      {/* Empty for visual balance */}
                    </div>
                  </div>
                  <div className="hidden md:block absolute left-1/2 bottom-0 transform -translate-x-6 text-primary-600 text-3xl">
                    <FaLongArrowAltRight className="transform rotate-90" />
                  </div>
                </div>
              </Link>
              
              {/* Step 3 */}
              <Link href="/analysis">
                <div className="relative mb-12">
                  <div className="flex md:flex-row flex-col items-center">
                    <div className="md:w-1/2 flex justify-end pr-8 pb-4 md:pb-0">
                      <div className="card hover:border-primary-500 hover:border-2 cursor-pointer md:max-w-md w-full">
                        <div className="flex items-center mb-4">
                          <div className="flex-shrink-0">
                            <div className="flex items-center justify-center h-12 w-12 rounded-md bg-primary-500 text-white">
                              <FaChartLine />
                            </div>
                          </div>
                          <h3 className="ml-4 text-lg leading-6 font-medium text-gray-900">Step 3: Anomaly Detection</h3>
                        </div>
                        <p className="text-base text-gray-500">
                          Detect anomalies in your log data using machine learning algorithms including One-Class SVM and DBSCAN.
                        </p>
                      </div>
                    </div>
                    
                    <div className="hidden md:flex absolute left-1/2 transform -translate-x-1/2 items-center justify-center h-10 w-10 rounded-full bg-primary-500 text-white font-bold">3</div>
                    
                    <div className="md:w-1/2 pl-8">
                      {/* Empty for visual balance */}
                    </div>
                  </div>
                  <div className="hidden md:block absolute right-1/2 bottom-0 transform translate-x-6 text-primary-600 text-3xl">
                    <FaLongArrowAltRight className="transform rotate-90" />
                  </div>
                </div>
              </Link>
              
              {/* Step 4 */}
              <Link href="/analysis">
                <div className="relative">
                  <div className="flex md:flex-row flex-col items-center">
                    <div className="md:w-1/2 pl-8 order-2 md:order-none">
                      <div className="card hover:border-primary-500 hover:border-2 cursor-pointer md:max-w-md w-full">
                        <div className="flex items-center mb-4">
                          <div className="flex-shrink-0">
                            <div className="flex items-center justify-center h-12 w-12 rounded-md bg-primary-500 text-white">
                              <FaRegLightbulb />
                            </div>
                          </div>
                          <h3 className="ml-4 text-lg leading-6 font-medium text-gray-900">Step 4: Insights</h3>
                        </div>
                        <p className="text-base text-gray-500">
                          Gain valuable insights from your log data with visualizations and actionable intelligence.
                        </p>
                      </div>
                    </div>
                    
                    <div className="hidden md:flex absolute left-1/2 transform -translate-x-1/2 items-center justify-center h-10 w-10 rounded-full bg-primary-500 text-white font-bold">4</div>
                    
                    <div className="md:w-1/2 pr-8 order-1 md:order-none">
                      {/* Empty for visual balance */}
                    </div>
                  </div>
                </div>
              </Link>
            </div>
          </div>
          
          <div className="mt-16 text-center">
            <Link href="/upload">
              <button className="btn-primary text-lg px-8 py-3">
                Start Analyzing Logs
              </button>
            </Link>
          </div>
        </div>
      </div>
    </main>
  )
} 