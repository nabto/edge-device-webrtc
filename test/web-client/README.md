# WebRTC client example for browsers

## Usage

From a terminal:
```
sudo npm install -g browserify
npm install
browserify nabtoWebrtc.js -o nabtoBundle.js
python3 -m http.server -d .
```

In a browser go to localhost:8000


## code flow in videoClient.js

`videoClient.js` is the main script running the WebRTC application


