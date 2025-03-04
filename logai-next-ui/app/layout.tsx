import type { Metadata } from 'next'
import '../styles/globals.css'
import { ToastContainer } from 'react-toastify'
import 'react-toastify/dist/ReactToastify.css'

export const metadata: Metadata = {
  title: 'LogAI - Log Analysis and Anomaly Detection',
  description: 'High-performance log analysis and anomaly detection platform',
}

export default function RootLayout({
  children,
}: {
  children: React.ReactNode
}) {
  return (
    <html lang="en">
      <body>
        <ToastContainer position="top-right" autoClose={5000} />
        {children}
      </body>
    </html>
  )
} 