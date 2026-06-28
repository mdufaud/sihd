var websocket = new WebSocket("ws://localhost:3000", "websocket-protocol");

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

function json_request(type, url, obj)
{
    var xhr = new XMLHttpRequest();
    xhr.open(type, url, true);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.send(JSON.stringify(obj));
}

function text_request(type, url, value)
{
    var xhr = new XMLHttpRequest();
    xhr.open(type, url, true);
    xhr.setRequestHeader('Content-Type', 'text/plain');
    xhr.send(value);
}

text_request("GET", "webservice/get", null);
json_request("POST", "webservice/post", { data: 42 });
text_request("POST", "webservice/post", "hello post world");
text_request("PUT", "webservice/put", "hello put world");
// DELETE request MUST be empty
text_request("DELETE", "webservice/delete", null);