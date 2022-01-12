var websocket = new WebSocket("ws://localhost:3000", ["proto-one", "proto-two"]);

websocket.onopen = function(event)
{
    console.log("Socket opened");
    websocket.send("hello from client");
}

websocket.onclose = function(event)
{
    console.log("Socket closed");
}

websocket.onmessage = function(event)
{
    console.log("Server said: ", event.data);
}