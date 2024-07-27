let submit = document.getElementById("submit");

const atmosim = new Worker("./worker_integration.js");
atmosim.addEventListener("message", (e) => {
    let message = e.data;
    if(message[0] == "output") {
        console.log(message[1]);
    }
});

submit.addEventListener("click", e => {
    atmosim.postMessage(["compute"]);
});