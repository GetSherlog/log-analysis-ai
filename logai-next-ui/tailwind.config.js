/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    './app/**/*.{js,ts,jsx,tsx,mdx}',
    './components/**/*.{js,ts,jsx,tsx,mdx}',
  ],
  theme: {
    extend: {
      colors: {
        primary: {
          50: '#f0f9f0',
          100: '#dcf5dc',
          200: '#b8eab8',
          300: '#7dd87d',
          400: '#4cc04c',
          500: '#2ea02e',
          600: '#1e891e',
          700: '#196619',
          800: '#144f14',
          900: '#0e3b0e',
        },
      },
    },
  },
  plugins: [],
} 