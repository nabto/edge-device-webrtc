function parseString() {
  const devStr = document.getElementById("devicestring").value;
  if (devStr.length == 0) {
    boxLog("Invalid device string!");
    return false;
  }

  let params = devStr.split(',');
  for (let p of params) {
    let [key, value] = p.split(':');
    if (key == "productid") {
      productId = value;
      localStorage.setItem("productId", productId);
    } else if (key == "deviceid") {
      deviceId = value;
      localStorage.setItem("deviceId", deviceId);
    } else if (key == "sct") {
      sct = value;
      localStorage.setItem("sct", sct);
    }
  }
  updateConfig();
}

function updateConfig() {
  const prod = localStorage.getItem("productId");
  if (prod) {
    document.getElementById("product").value = prod;
    productId = prod;
  }
  const dev = localStorage.getItem("deviceId");
  if (dev) {
    document.getElementById("device").value = dev;
    deviceId = dev;
  }
  const sctLoc = localStorage.getItem("sct");
  if (sctLoc) {
    document.getElementById("sct").value = sctLoc;
    sct = sctLoc;
  }
};

addEventListener("load", (event) => {
  updateConfig();
});

function storeInfo() {
  productId = document.getElementById("product").value;
  deviceId = document.getElementById("device").value;
  sct = document.getElementById("sct").value;

  localStorage.setItem("productId", productId);
  localStorage.setItem("deviceId", deviceId);
  localStorage.setItem("sct", sct);
  updateConfig();
}

function boxLog(msg) {
  var listElem = document.querySelector(".logbox");

  var item = document.createElement("li");
  item.appendChild(document.createTextNode(msg));
  listElem.appendChild(item);
}
