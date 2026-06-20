const fs = require('fs');
const zlib = require('zlib');
const path = require('path');

function compressFile(srcName, destName) {
    const srcPath = path.join(__dirname, '../main', srcName);
    const destPath = path.join(__dirname, '../main', destName);
    
    console.log(`Compressing ${srcName} -> ${destName} ...`);
    try {
        const content = fs.readFileSync(srcPath);
        const compressed = zlib.gzipSync(content);
        fs.writeFileSync(destPath, compressed);
        console.log(`Success! ${srcName}: ${content.length} bytes -> ${destName}: ${compressed.length} bytes.`);
    } catch (err) {
        console.error(`Compression of ${srcName} failed:`, err);
        process.exit(1);
    }
}

compressFile('index.html', 'index.html.gz');
compressFile('config.html', 'config.html.gz');
