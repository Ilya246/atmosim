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
        let data = message[1];
        console.log(data);

        console.log("worker: starting atmosim...");
        Module.callMain([
            "--gas1", data.gas1,
            "--gas2", data.gas2,
            "--gas3", data.gas3,
            "--mixt1", data.mixt1,
            "--mixt2", data.mixt2,
            "--thirt1", data.thirt1,
            "--thirt2", data.thirt2,
            "--doretest", data.doretest
        ]);
        console.log("worker: atmosim finished");
    }
}