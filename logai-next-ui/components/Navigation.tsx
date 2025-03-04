import Link from 'next/link'
import { FaHome, FaUpload, FaChartLine, FaCog } from 'react-icons/fa'

export default function Navigation() {
  return (
    <nav className="bg-white shadow-md">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="flex justify-between h-16">
          <div className="flex">
            <div className="flex-shrink-0 flex items-center">
              <Link href="/" className="text-primary-600 font-bold text-xl">Sherlog</Link>
            </div>
            <div className="hidden sm:ml-6 sm:flex sm:space-x-8">
              <Link href="/" className="border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium">
                <FaHome className="mr-1" /> Home
              </Link>
              <Link href="/upload" className="border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium">
                <FaUpload className="mr-1" /> Upload
              </Link>
              <Link href="/analysis" className="border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium">
                <FaChartLine className="mr-1" /> Analysis
              </Link>
              <Link href="/settings" className="border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium">
                <FaCog className="mr-1" /> Settings
              </Link>
            </div>
          </div>
          <div className="flex items-center">
            <div className="ml-3 relative">
              <div>
                <button className="bg-primary-500 flex text-sm rounded-full focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-primary-500 text-white px-4 py-2 font-medium">
                  Documentation
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>
    </nav>
  )
} 