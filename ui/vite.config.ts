import react from '@vitejs/plugin-react';
import { defineConfig } from 'vite';

// During development (npm run dev) the page is served from this PC and its API
// calls are passed on to a real board. Set EYE_DEVICE to the board's address;
// the default is the prop's own access point.
const device = process.env.EYE_DEVICE ?? '192.168.4.1';

export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      '/api': { target: `http://${device}`, changeOrigin: true },
      '/ws': { target: `ws://${device}`, ws: true, changeOrigin: true },
    },
  },
  build: {
    // One script and one stylesheet, which scripts/embed_ui.py folds into a
    // single HTML file for the firmware.
    target: 'es2022',
    cssCodeSplit: false,
    assetsInlineLimit: 1024 * 1024,
    modulePreload: { polyfill: false },
    reportCompressedSize: false,
  },
});
