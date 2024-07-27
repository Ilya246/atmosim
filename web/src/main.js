let outputbox = document.getElementById("outputbox");
let submit = document.getElementById("submit");

const atmosim = new Worker("./worker_integration.js");

let currentLine = "";
atmosim.addEventListener("message", (e) => {
    let message = e.data;
    if(message[0] == "output") {
        let c = message[1];
        if(c !== "\n") {
            currentLine = `${currentLine}${c}`;
        } else {
            let p = document.createElement("p");
            p.innerText = currentLine;
            outputbox.appendChild(p);
            currentLine = "";
        }
    }
});

let canistergas1 = document.getElementById("canistergas1");
let canistergas2 = document.getElementById("canistergas2");
let tankgas = document.getElementById("tankgas");
let mixt1 = document.getElementById("mixt1");
let mixt2 = document.getElementById("mixt2");
let thirt1 = document.getElementById("thirt1");
let thirt2 = document.getElementById("thirt2");
let ticks = document.getElementById("ticks");
let doretest = document.getElementById("doretest");

submit.addEventListener("click", e => {
    atmosim.postMessage(["compute", {
        gas1: canistergas1.value,
        gas2: canistergas2.value,
        gas3: tankgas.value,
        mixt1: mixt1.value,
        mixt2: mixt2.value,
        thirt1: thirt1.value,
        thirt2: thirt2.value,
        ticks: ticks.value,
        doretest: doretest.checked ? "y" : "n"
    }]);
});