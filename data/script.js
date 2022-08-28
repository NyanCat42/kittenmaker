
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

function getValues(){
    websocket.send("getValues");
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    getValues();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function updateInput(element) {
    var inputNumber = element.id.charAt(element.id.length-1);
    var inputValue = document.getElementById(element.id).value;
    document.getElementById("inputValue"+inputNumber).innerHTML = inputValue;
    console.log(inputValue);
    websocket.send(inputNumber+"s"+inputValue.toString());
}

function buttonPress(whatButton) {
    websocket.send(whatButton+"s0".toString());
}

function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);
    
    document.getElementById("fileName").innerHTML = myObj["fileName"];
    document.getElementById("estimatedTime").innerHTML = myObj["estimatedTime"];
    document.getElementById("progressBar").style = myObj["progressBar"];

    document.getElementById("input1").value = myObj["inputValue1"];
    document.getElementById("inputValue1").innerHTML = myObj["inputValue1"];
    document.getElementById("input2").value = myObj["inputValue2"];
    document.getElementById("inputValue2").innerHTML = myObj["inputValue2"];
    document.getElementById("input3").value = myObj["inputValue3"];
    document.getElementById("inputValue3").innerHTML = myObj["inputValue3"];
    document.getElementById("input4").value = myObj["inputValue4"];
    document.getElementById("inputValue4").innerHTML = myObj["inputValue4"];
    document.getElementById("input5").value = myObj["inputValue5"];
    document.getElementById("inputValue5").innerHTML = myObj["inputValue5"];

}
