

import { WebSocketServer } from './websocket_server';

async function main() {
  // TODO: this crashes if a device is closed with an active connection
  let serve = new WebSocketServer();
}

try {
  main();
} catch (e) {
  console.log("FAILURE: ", e);
}
