import { createServer, IncomingMessage, ServerResponse, Server } from 'http';

import { server } from 'websocket'
import { WebSocketConnection } from './websocket_connection';

export class WebSocketServer {
  port = 6503;
  webServer: Server<typeof IncomingMessage, typeof ServerResponse>;
  wsServer: server;
  connectionArray: WebSocketConnection[] = [];

  constructor() {
    let self = this;
    this.webServer = createServer((request: IncomingMessage, response: ServerResponse) => {
      response.writeHead(404);
      response.end();
    });

    this.webServer.listen(6503, function () {
      console.log("Server is listening on port 6503");
    });

    this.wsServer = new server({
      httpServer: this.webServer,
      autoAcceptConnections: false
    });

    if (!this.wsServer) {
      console.log("ERROR: Unable to create WebSocket server!");
    }

    this.wsServer.on('request', function(request) {
      // Accept the request and get a connection.
      var connection = request.accept("json", request.origin);

      // Add the new connection to our list of connections.

      self.connectionArray.push(new WebSocketConnection(self, connection));
    });
  }

  removeConnection(conn: WebSocketConnection)
  {
    const index = this.connectionArray.indexOf(conn, 0);
    if (index > -1) {
       this.connectionArray.splice(index, 1);
    }

  }
}
