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

submit.addEventListener("click", e => {
    atmosim.postMessage(["compute"]);
});