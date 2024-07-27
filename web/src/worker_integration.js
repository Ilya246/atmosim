// web worker main code
// we split this into a worker so that the relatively heavy atmosim binary isn't slowing down the UI thread.
// this makes the page and browser itself feel a lot more responsive while atmosim is computing.

var Module = {
    noInitialRun: true,

    preInit: () => {
        function stdin() {
            return '\0'.charCodeAt(0);
        }
        
        function stdout(code) {
            postMessage(["output", String.fromCharCode(code)]);
        }

        function stderr(code) {
            postMessage(["output", String.fromCharCode(code)]);
        }

        FS.init(stdin, stdout, stderr);
    }
}

importScripts("./atmosim.js");

onmessage = (e) => {
    let message = e.data;
    if(message[0] == "compute") {
        console.log("worker: starting atmosim...");
        Module.callMain([
            "--gas1", "plasma",
            "--gas2", "tritium",
            "--gas3", "oxygen",
            "--mixt1", "70",
            "--mixt2", "1000",
            "--thirt1", "200",
            "--thirt2", "300",
            "--doretest", "n"
        ]);
    }
}