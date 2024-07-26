let box = document.getElementById("box");
let submit = document.getElementById("submit");
box.innerText = "";

var Module = {
    noInitialRun: false,
    arguments: [
        "--ticks", "150",
        "--gas1", "plasma",
        "--gas2", "tritium",
        "--gas3", "oxygen",
        "--mixt1", "70",
        "--mixt2", "1000",
        "--thirt1", "200",
        "--thirt2", "300",
        "--doretest", "n",
    ],

    preInit: () => {
        function stdin() {
            return '\0'.charCodeAt(0);
        }
        
        function stdout(code) {
            box.innerText = String.fromCharCode(code);
        }

        function stderr(code) {
            box.innerText = String.fromCharCode(code);
        }

        FS.init(stdin, stdout, stderr);
    }
}

submit.addEventListener("click", e => {
    Module.arguments = [
        "--gas1", "plasma",
        "--gas2", "tritium",
        "--gas3", "oxygen",
        "--mixt1", "70",
        "--mixt2", "1000",
        "--thirt1", "200",
        "--thirt2", "300",
        "--doretest", "n"
    ];

    console.log("running...");
    Module.run();
});