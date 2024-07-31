let outputbox = document.getElementById("outputbox");
let submit = document.getElementById("submit");

const atmosim = new Worker("./worker_integration.js");

let allText = [];
let currentLine = "";
atmosim.addEventListener("message", (e) => {
    let message = e.data;
    if(message[0] == "output") {
        let c = message[1];
        if(c !== "\n") {
            currentLine = `${currentLine}${c}`;
        } else {
            allText.push(`${currentLine}`); // wrapper to duplicate string instead of ref
            let p = document.createElement("p");
            p.innerText = currentLine;
            outputbox.appendChild(p);
            currentLine = "";
        }
    } else if(message[0] == "finish") {
        let result = allText.map(x => x.split("\t").join(""));
        let blocks = {};
        let currentlyAppending = "";
        for(let i in result) {
            let line = result[i];
            if(line.endsWith(": {")) { // TANK etc
                currentlyAppending = line.split(":")[0];
                blocks[currentlyAppending] = {};
            } else if(line == "}") {
                currentlyAppending = "";
            } else {
                if(currentlyAppending) {
                    let key = line.split(": ")[0]; // initial state etc
                    let content = line.split(": ").slice(1, line.split(": ").length).join(": ");
                    if(content.startsWith("[")) {
                        // array
                        let array = content.slice(1, content.length - (content.endsWith(";") ? 2 : 1)).split("|").map(x => x.trim());
                        blocks[currentlyAppending][key] = array;
                    } else {
                        // single value
                        blocks[currentlyAppending][key] = content;
                    }
                }
            }
        }

        displayOutput(blocks);
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

let resultbox = document.getElementById("resultbox");
let oCanistermix = document.getElementById("o-canistermix");
let oLeastmols = document.getElementById("o-leastmols");
let oCanistertemp = document.getElementById("o-canistertemp");
let oCanisterpressure = document.getElementById("o-canisterpressure");
let oTankmix = document.getElementById("o-tankmix");
let oTankmols = document.getElementById("o-tankmols");
let oTanktemp = document.getElementById("o-tanktemp");
let oTankpressure = document.getElementById("o-tankpressure");
let oTimer = document.getElementById("o-timer");
let oRadius = document.getElementById("o-radius");

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

function displayOutput(data) {
    console.log(data);

    // canister
    // 60.633331%:39.366669%=1.540220 plasma:tritium
    {
        let mixdata = data["REQUIREMENTS"]["mix-canister (fuel)"][0];
        let [percentdata, typedata] = mixdata.split(" ");

        percentdata = percentdata.split("=")[0];
        let [firstPercent, secondPercent] = percentdata.split(":").map(x => x.substr(0, x.length - 1)).map(x => parseFloat(x));

        let [firstGas, secondGas] = typedata.split(":");

        oCanistermix.innerText = `Mix: %${firstPercent.toFixed(4)} ${firstGas} : %${secondPercent.toFixed(4)} ${secondGas}`
    }

    // totally messed up, atmosim output is not very nice to parse
    {
        let firstLeastMols = data["REQUIREMENTS"]["mix-canister (fuel)"][3].split(": [")[1];
        let secondLeastMols = data["REQUIREMENTS"]["mix-canister (fuel)"][4];

        oLeastmols.innerText = `Least mols: ${firstLeastMols}, ${secondLeastMols}`;
    }

    // temp 398.105682K
    {
        let tempStep1 = data["REQUIREMENTS"]["mix-canister (fuel)"][1].split(" ")[1];
        temp = parseFloat(tempStep1.slice(0, tempStep1.length - 1));

        oCanistertemp.innerText = `Temp: ${temp.toFixed(4)}K`;
    }

    // release pressure 693.503296kPa
    {
        let pressureStep1 = data["REQUIREMENTS"]["mix-canister (fuel)"][2];
        let pressure = parseFloat(pressureStep1.slice(0, pressureStep1.length - 3));

        oCanisterpressure.innerText = `Canister release pressure set to ${pressure.toFixed(4)}KPa`;
    }

    // tank
    // mix
    {
        let mix = data["REQUIREMENTS"]["third-canister (primer)"][2].split(" ")[2];

        oTankmix.innerText = `Mix: ${mix}`;
    }

    // temp 202.005005K
    {
        let tempStep1 = data["REQUIREMENTS"]["third-canister (primer)"][0].split(" ")[1];
        let temp = parseFloat(tempStep1.slice(0, tempStep1.length - 1));

        oTanktemp.innerText = `Temp: ${temp.toFixed(4)}K`;
    }

    // least-mols: 1115.087769mol oxygen
    {
        let leastMols = data["REQUIREMENTS"]["third-canister (primer)"][2].split(": ")[1];

        oTankmols.innerText = `Least mols: ${leastMols}`;
    }

    // pressure 1337.757357kPa
    {
        let pressureStep1 = data["REQUIREMENTS"]["third-canister (primer)"][1].split(" ")[1];
        let pressure = parseFloat(pressureStep1.slice(0, pressureStep1.length - 3));

        oTankpressure.innerText = `Tank pressure: ${pressure.toFixed(4)}KPa`;
    }

    // Generic results
    // ticks 30t
    {
        let tickStep1 = data["TANK"]["end state"][0].split(" ")[1];
        let ticks = parseFloat(tickStep1.slice(0, tickStep1.length - 1));

        oTimer.innerText = `Time before detonation: ${ticks} ticks (${ticks / 2} seconds)`;
    }

    // radius 11.661351til
    {
        let radiusStep1 = data["TANK"]["end state"][3].split(" ")[1];
        let radius = parseFloat(radiusStep1.slice(0, radiusStep1.length - 3));

        oRadius.innerText = `Explosive radius: ${radius.toFixed(2)} tiles`;
    }

    resultbox.classList.remove("hidden");
}