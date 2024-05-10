// Example of WASM hook

var reset;
var add_given;
var add_deduction;
var erase_deductions_after;
var deduction_hint;
var set_closure;
var closure_hint;
var hints_this_row;

/*
Row format: [
    // HTML storage
    root : element
    referBox : element
    textField : element
    lhsBoxes : [element]
    rhsBoxes : [element]
    buttons : [element]

    //Data storage
    rule : number,
    references : [number]
    lhsBitfield : number,
    rhsBitfield : number,
    ...?
]
*/

activeRow = 0;
derivationRows = []; // store data to be submitted
derivationRows.push([]);
activeTable = 0;
derivationTable = []
derivationTable.push(document.getElementById("derivation" + activeTable.toString()));

numAttributes = 5; // grab from database
givens = [
    1, 2,
    2, 4,
    6, 8,
    14, 16
]
lq_target = [1, 31]
/*
numAttributes = 11;
givens = [
    256, 1024,
    257, 98,
    260, 25,
    322, 516,
    1024, 129,
]
lq_target = [257, 128] // AI -> H
//lq_target = [5, 1024] // AC -> K
*/

closure_start = 1;
//for question type 1 current closure comes from database, for 2 and 3 it is input by user when creating a new table
questionType = 0; //grab from database. 0 = logical consequence, 1 = attribute closure, 2 = candidate key exhaustive, 3 = candidate key heuristic

function bitfield_letters(bitfield) {
    var str = "";
    for (var i = 0; i < numAttributes; i++) {
        if (bitfield & (1 << i)) str += String.fromCharCode([65 + i]);
    }
    return str;
}

function string_tobitfield(string){
    var bitfield = 0;
    for(var i = 0; i<string.length; i++) {
        console.log((string.charCodeAt(i) - 65))
        bitfield += (1 << (string.charCodeAt(i) - 65));
    }
    return bitfield;
}

function fd_bitfield_tostring(first_bits, second_bits) {
    let first_str = bitfield_letters(first_bits);
    let second_str = bitfield_letters(second_bits);
    return first_str + " -> " + second_str;
}

function getActiveOuterTable() {
    return document.getElementById("table" + activeTable);
}

function getActiveRow() {
    return derivationRows[activeTable][derivationRows[activeTable].length - 1];
}
function setActiveRow(row) {
    derivationRows[activeTable][derivationRows[activeTable].length - 1] = row;
}

function onRowModified() {
    var row = getActiveRow();
    var reason_string = '';
    Array.from(document.getElementsByClassName("reason_radio")).forEach(function(item, index) { if(item.checked) { row.rule = index; reason_string = item.id; } });
    
    if(questionType == 1){
        row.lhsBitfield = 0;
        row.lhsBoxes.forEach(function(item, index) { if (item.checked) row.lhsBitfield |= 1 << index; });
    }

    row.rhsBitfield = 0;
    row.rhsBoxes.forEach(function(item, index) { if (item.checked) row.rhsBitfield |= 1 << index; });

    row.references = [];
    Array.from(document.getElementsByClassName("given_box")).forEach(function(item, index) { if(item.checked) row.references.push(index + 1); });
    derivationRows[activeTable].forEach(function(item, index) { if(item.referBox.checked) row.references.push(index + (givens.length/2) + 1); });

    for(var i = 0; i < row.references.length; i++){
        reason_string += ", " + row.references[i];
    }
    row.textField.nodeValue = reason_string;
    setActiveRow(row);
}

function resetRowSpecificOpts() {
    // reset checkboxes and radios appropriately
    Array.from(document.getElementsByClassName("reason_radio")).forEach(function(item) { item.checked = false; });
    Array.from(document.getElementsByClassName("given_box")).forEach(function(item) { item.checked = false; });
    derivationRows[activeTable].forEach(function(item) { item.referBox.checked = false; });
}

function createRow() {
    var root = derivationTable[activeTable].appendChild(document.createElement("div"));
    root.setAttribute("class","row");
    var left = root.appendChild(document.createElement("div"));
    left.setAttribute("class", "col-8");
    var middle = root.appendChild(document.createElement("div"));
    middle.setAttribute("class", "col-2");
    var right = root.appendChild(document.createElement("div"));
    right.setAttribute("class", "col-2");
    var tableLeft = left.appendChild(document.createElement("table").insertRow());
    var tableMiddle = middle.appendChild(document.createElement("table").insertRow());
    var tableRight = right.appendChild(document.createElement("table").insertRow());

    let rowNumber = (derivationRows[activeTable].length + givens.length / 2 + 1).toString();
    let cell0 = tableLeft.insertCell();
    cell0.appendChild(document.createTextNode(rowNumber + ". "));
    var referBox = cell0.appendChild(document.createElement("input"));
    referBox.setAttribute("class", "form-check-input");
    referBox.setAttribute("type", "checkbox");
    referBox.setAttribute("onclick", "onRowModified()");
    referBox.disabled = true;

    var lhsBoxes = [];
    if(questionType == 0){
        for (var i = 0; i < numAttributes; i++) {
            let cellN = tableLeft.insertCell();
            cellN.appendChild(document.createTextNode(String.fromCharCode(65 + i)));
            lhsBoxes.push(cellN.appendChild(document.createElement("input")));
            lhsBoxes[i].setAttribute("class", "form-check-input");
            lhsBoxes[i].setAttribute("type", "checkbox");
            lhsBoxes[i].setAttribute("onclick", "onRowModified()");
        }
    } else {
        let cellN = tableLeft.insertCell();
        cellN.appendChild(document.createTextNode("{" + bitfield_letters(closure_start) + "}+F"));
    }

    if(questionType == 0){
        tableLeft.insertCell(-1).appendChild(document.createTextNode("->"));
    } else {
        tableLeft.insertCell(-1).appendChild(document.createTextNode("="));
    }
    

    var rhsBoxes = [];
    for (var i = 0; i < numAttributes; i++) {
        let cellN = tableLeft.insertCell();
        cellN.appendChild(document.createTextNode(String.fromCharCode(65 + i)));
        rhsBoxes.push(cellN.appendChild(document.createElement("input")));
        rhsBoxes[i].setAttribute("class", "form-check-input")
        rhsBoxes[i].setAttribute("type", "checkbox");
        rhsBoxes[i].setAttribute("onclick", "onRowModified()");
    }

    var reasoningText = tableMiddle.insertCell().appendChild(document.createTextNode(""));
    
    var buttons = [];
    buttons.push(tableRight.insertCell());
    buttons.push(buttons[0].appendChild(document.createElement("button")));
    buttons[1].setAttribute("onclick", "duplicateRow()");
    buttons[1].appendChild(document.createTextNode("Next"));
    buttons.push(buttons[0].appendChild(document.createElement("button")));
    buttons[2].setAttribute("onclick", "finishDerivation(false)");
    buttons[2].appendChild(document.createTextNode("Finish"));
    
    var errorBox = tableRight.insertCell();

    derivationRows[activeTable].push(
        {
            "root": root,
            "referBox": referBox,
            "textField": reasoningText,
            "lhsBoxes": lhsBoxes,
            "rhsBoxes": rhsBoxes,
            "buttons": buttons,
            "rule": 0,
            "references": [],
            "lhsBitfield": closure_start,
            "rhsBitfield": 0,
            "errorBox": errorBox
        }
    );
    hints_this_row = 0;
}

function disableLastRow() {
    let row = getActiveRow();

    var result = 0
    if (questionType == 0) {
        var ref1 = row.references.length > 0 ? row.references[0] - 1 : 0;
        var ref2 = row.references.length > 1 ? row.references[1] - 1 : 0;
        result = add_deduction(row.lhsBitfield, row.rhsBitfield, row.rule, ref1, ref2)
    }
    else if (questionType == 1) {
        var ref1 = row.references.length > 0 ? row.references[0] - 1 : 0;
        result = set_closure(row.rhsBitfield, ref1)
    }
    
    if (result < 0) {
        var img = document.createElement("img");
        img.src = "error.webp";
        img.setAttribute("style", "height:32px;width:40px;");
        row.errorBox.appendChild(img);
    }
    if (result == -1)
        alert("Prerequisites not met for deduction type.");
    if (result == -2)
        alert("References out of range, somehow.");
    if (result == -3)
        alert("Expected a different dependency based on deduction type and references.");

    row.lhsBoxes.forEach(function(item) { item.disabled = true; });
    row.rhsBoxes.forEach(function(item) { item.disabled = true; });
    row.referBox.disabled = false;

    // change done button to undo, swap function from onRowModified() to revertToRow(n)
    row.buttons[0].removeChild(row.buttons[1]);
    row.buttons[0].removeChild(row.buttons[2]);
    row.buttons[1] = (row.buttons[0].appendChild(document.createElement("button")))
    row.buttons[1].setAttribute("onclick", "revertToRow(" + (derivationRows[activeTable].length).toString() + ")");
    row.buttons[1].appendChild(document.createTextNode("Undo"));
    setActiveRow(row);
}

function duplicateRow() {
    disableLastRow();
    resetRowSpecificOpts();
    createRow();
}

function revertToRow(n) {
    erase_deductions_after(n + givens.length / 2 - 1);

    for (var i = n - 1; i < derivationRows[activeTable].length; i++) {
        derivationTable[activeTable].removeChild(derivationRows[activeTable][i].root);
    }
    derivationRows[activeTable] = derivationRows[activeTable].slice(0, n - 1);
    createRow();
    resetRowSpecificOpts();
    
    // In case of undoing finish
    Array.from(document.getElementsByClassName("reason_radio")).forEach(function(item) { item.disabled = false; });
    Array.from(document.getElementsByClassName("given_box")).forEach(function(item) { item.disabled = false; });
    derivationRows[activeTable].forEach(function(item) { item.referBox.disabled = false; });
    derivationRows[activeTable][n-1].referBox.disabled = true;
}

function finishDerivation(delete_last) {
    if (delete_last) {
        derivationTable[activeTable].removeChild(derivationRows[activeTable][derivationRows[activeTable].length - 1].root);
        derivationRows[activeTable] = derivationRows[activeTable].slice(0, derivationRows[activeTable].length - 1);
    } else
        disableLastRow();
    resetRowSpecificOpts();

    var outerTable = getActiveOuterTable();
    Array.from(outerTable.getElementsByClassName("reason_radio")).forEach(function(item) { item.disabled = true; });
    Array.from(outerTable.getElementsByClassName("given_box")).forEach(function(item) { item.disabled = true; });
    derivationRows[activeTable].forEach(function(item) { item.referBox.disabled = true; });
}

function createGivens(addBox) {
    var table = document.createElement('table');
    for (var i = 0; i < givens.length / 2; i++){
        var row = table.insertRow(i);
        var cell = row.insertCell(0);
        if(!addBox){
            cell.setAttribute("class", "border border-secondary");
        }
        var text = document.createTextNode((i + 1).toString() + ". " + fd_bitfield_tostring(givens[2 * i], givens[2 * i + 1]) + "  ");
        if (addBox) {
            var checkbox = document.createElement("input");
            checkbox.setAttribute("class", "given_box form-check-input");
            checkbox.setAttribute("type", "checkbox");
            checkbox.setAttribute("onclick", "onRowModified()");
            cell.appendChild(text);
            cell.appendChild(checkbox);
        } else {
            cell.appendChild(text);
        }

    }
    return table;
}

function do_hint(level) {
    if (questionType == 0) {
        var type = deduction_hint(lq_target[0], lq_target[1], 0);
        if (type == -1)
            return 1;
        var ref1 = deduction_hint(lq_target[0], lq_target[1], 1);
        var ref2 = deduction_hint(lq_target[0], lq_target[1], 2);
        var lhs = deduction_hint(lq_target[0], lq_target[1], 3);
        var rhs = deduction_hint(lq_target[0], lq_target[1], 4);

        function check_ref(n, keep_old) {
            if (n < 0) return 0;
            Array.from(document.getElementsByClassName("given_box")).forEach(function(item, index) { item.checked = item.checked && keep_old || index == n; });
            derivationRows[activeTable].forEach(function(item, index) { item.referBox.checked = item.referBox.checked && keep_old || index + (givens.length/2) == n; });
        }

        if (level == 0) {
            if (hints_this_row == 0)
                Array.from(document.getElementsByClassName("reason_radio")).forEach(function(item, index) { item.checked = index == type; });
            else if (hints_this_row == 1)
                check_ref(ref1, false);
            else if (hints_this_row == 2)
                check_ref(ref2, true);
            else if (hints_this_row == 3)
                getActiveRow().lhsBoxes.forEach(function(item, index) { item.checked = (lhs & (1 << index)) != 0; });
            else if (hints_this_row == 4)
                getActiveRow().rhsBoxes.forEach(function(item, index) { item.checked = (rhs & (1 << index)) != 0; });
            hints_this_row++;
            onRowModified();
            if (hints_this_row == 6)
                duplicateRow();
        }
        else if (level == 1) {
            do do_hint(0);
            while (hints_this_row < 5);
        }
        else if (level == 2) {
            while (do_hint(0) != 1) { }
            finishDerivation(true);
        }
        return 0;
    }
    else if (questionType == 1) {
        var val = closure_hint(0);
        var ref = closure_hint(1);

        function check_ref(n, keep_old) {
            if (n < 0) return 0;
            Array.from(document.getElementsByClassName("given_box")).forEach(function(item, index) { item.checked = item.checked && keep_old || index == n; });
            derivationRows[activeTable].forEach(function(item, index) { item.referBox.checked = item.referBox.checked && keep_old || index + (givens.length/2) == n; });
        }

        if (level == 0) {
            if (hints_this_row == 0)
                check_ref(ref, false);
            else if (hints_this_row == 1)
                getActiveRow().rhsBoxes.forEach(function(item, index) { item.checked = (val & (1 << index)) != 0; });
            hints_this_row++;
            onRowModified();
            if (hints_this_row == 2)
                duplicateRow();
        }
        else if (level == 1) {
            do do_hint(0);
            while (hints_this_row < 2);
        }
        else if (level == 2) {
            while (do_hint(0) != 1) { }
            finishDerivation(true);
        }
        return 0;
    }
}

function wasm_init() {
    if (!runtimeInitialized) {
        setTimeout(wasm_init, 100);
        return;
    }
    console.log("WASM initialized")
    reset = Module.cwrap("reset", null, ["number", "number"]);
    add_given = Module.cwrap("add_given", null, ["number", "number"])
    add_deduction = Module.cwrap("add_deduction", "number", ["number", "number", "number", "number", "number"])
    erase_deductions_after = Module.cwrap("erase_deductions_after", null, ["number"])
    deduction_hint = Module.cwrap("deduction_hint", "number", ["number", "number", "number"])
    set_closure = Module.cwrap("set_closure", "number", ["number", "number"])
    closure_hint = Module.cwrap("closure_hint", "number", ["number"])

    reset(questionType, numAttributes);
    for (i = 0; i < givens.length; i += 2) {
        add_given(givens[i], givens[i + 1]);
    }
    if (questionType == 1) {
        set_closure(closure_start, -1);
    }
}

//make new derivation table
function newTable(){
    if(activeTable != -1) finishDerivation(false);
    
    var input = document.getElementById("newAttributes");
    var bitfield = string_tobitfield(input.value);
    activeTable++;
    activeRow = 0;
    closure_start = bitfield;
    set_closure(closure_start, -1);
    
    var newTable = document.getElementById("copyTable").cloneNode(true);
    newTable.setAttribute("id","table" + activeTable);
    document.getElementById("tablesGoHere").appendChild(newTable);
    newTable.setAttribute("style", "display:;");
    
    var newDeriv = newTable.getElementsByClassName("derivation")[0];
    newDeriv.setAttribute("id", "derivation" + activeTable);
    derivationTable.push(newDeriv);
    var newGivens = newTable.getElementsByClassName("derivGivens")[0];
    newGivens.setAttribute("id", "derivGivens" + activeTable);
    newGivens.appendChild(createGivens(true));

    derivationRows.push([]);
    createRow();
}

//Basic startup function
function init(qType) {
    questionType = qType
    wasm_init();
    str = ""
    for (i = 0; i < numAttributes; i++) str += String.fromCharCode([65 + i]);
    document.getElementById("header").appendChild(document.createElement("p"))
        .appendChild(document.createTextNode("Let R be a relation with the scheme {" + str + "}, and F be a set of dependencies on R."));
    if (qType == 0) {
        document.getElementById("header").appendChild(document.createElement("p"))
            .appendChild(document.createTextNode("Prove that the statement " + fd_bitfield_tostring(lq_target[0], lq_target[1]) + " holds on F."));
    }
    else if (qType == 1) {
        document.getElementById("header").appendChild(document.createElement("p"))
            .appendChild(document.createTextNode("Find the attribute closure of " + bitfield_letters(closure_start) + " under F."));
    }
    else if (qType == 2) {
        document.getElementById("header").appendChild(document.createElement("p"))
            .appendChild(document.createTextNode("Find the set of candidate keys of R."));
    }
    document.getElementById("header").appendChild(document.createElement("p"))
        .appendChild(document.createTextNode("The given dependencies in F are listed below."));
    document.getElementById("header").appendChild(document.createElement("div")).appendChild(createGivens(false));
    document.getElementById("derivGivens0").appendChild(createGivens(true));
    createRow();

    if(questionType >= 2){
        document.getElementById("moreTables").setAttribute("style", "display:;");
        document.getElementById("table0").remove();
        derivationTable = [];
        derivationRows = [];
        activeTable = -1;
    }
}
