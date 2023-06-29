const Spake2Client = require("./spake2");
const BN = require('bn.js');


let wRef = '2f75327ccb81d0340b8ce0c313cba2cd8575103898605ac4d83dc58cfe661ef6';

let Xref = '049118d47a8689f459c241494d0a55484051d672ef97a8a56b297e10b739acab1a541f046a2e0262a001843c226fb2d729dad63951f6fd68f9603bab2ca86ded90';

let xRef = '4b9483e0d85e1f654a76f2c1d5a665870e2ed7cf6f6d737c121a85936e0ac279';
let Tref = '0453ed82d3c4f78d7c2bdbae2ed9e2ae5189027dd778b6f9dfad44fcc22883383ddd30047db60ea1b573c6b9e775c8365ef651954e112e91b392c5b721100066e2';

let Sref = '04febe61670bb3477f36b38fa3ee5454b1baf89d63c1216cbae81c7f5f377354f6df2d013368787ec16fe86cc9ee90bc4956645ee78db9e77f9c7216b218bcdf77';

let Kref = '04b24e742efc9b734baba01d5975924f7771cabcf4017adec2f54e90352038479142bfb93d4c901017a0c2c2aaca47ce7e1640d436ab7896615b89a1021fd7cb57';

let TTref = '00000020cff2f65cd103488b8cb2b93e838acc0f719d6deae37f8a4b74fa825244d28af80000002073e53042551c128a492cfd910b9ba67fffd2cab6c023b50c10992289f4c23d54000000410453ed82d3c4f78d7c2bdbae2ed9e2ae5189027dd778b6f9dfad44fcc22883383ddd30047db60ea1b573c6b9e775c8365ef651954e112e91b392c5b721100066e20000004104febe61670bb3477f36b38fa3ee5454b1baf89d63c1216cbae81c7f5f377354f6df2d013368787ec16fe86cc9ee90bc4956645ee78db9e77f9c7216b218bcdf770000004104b24e742efc9b734baba01d5975924f7771cabcf4017adec2f54e90352038479142bfb93d4c901017a0c2c2aaca47ce7e1640d436ab7896615b89a1021fd7cb57000000202f75327ccb81d0340b8ce0c313cba2cd8575103898605ac4d83dc58cfe661ef6';

let cliFpRef = 'cff2f65cd103488b8cb2b93e838acc0f719d6deae37f8a4b74fa825244d28af8';
let devFpRef = '73e53042551c128a492cfd910b9ba67fffd2cab6c023b50c10992289f4c23d54';

let KeyRef = 'a16f87a014f059baf4e122756025e8b5736fb9623a954a1d1bdd30ca2b5eda0d';


let s = new Spake2Client("foo", "FFzeqrpJTVF4");

console.log("G: ", s.G.getX().toString(16), s.G.getY().toString(16));
console.log("n: ", s.n.toString(16));
console.log("M: ", s.M.getX().toString(16), s.M.getY().toString(16));

s.forceXVal(xRef);//new BN(xRef, 16));

let T = s.calculateT();

if (wRef != s.toHexString(s.w)) {
  console.log("FAILURE in w: ");
  console.log(wRef, " != ");
  console.log(s.toHexString(s.w));
}

if (Xref != s.toHexString(Buffer.from(s.X.encode(true)))) {
  console.log("FAILURE in X: ");
  console.log(Xref, " != ");
  console.log(s.toHexString(Buffer.from(s.X.encode(true))));
}


let Thex = s.toHexString(T);

if (Tref != Thex) {
  console.log("FAILURE in T: ");
  console.log(Tref, " != ");
  console.log(Thex);
}

s.calculateKHex(Sref);

if (Kref != s.toHexString(Buffer.from(s.K.encode(true)))) {
  console.log("FAILURE in K: ");
  console.log(Kref, " != ");
  console.log(s.toHexString(Buffer.from(s.K.encode(true))));
}

s.calculateKey(cliFpRef, devFpRef);

let TT = s.toHexString(s.TT);
if (TTref != TT) {
  let is = [];
  for (let i = 0; i < TTref.length; i++) {
    if (TTref[i] != TT[i]) {
      is.push(i);
    }
  }
  console.log("FAILURE in TT at indexes: ", is);
  console.log(TTref, " != ");
  console.log(TT);
}


if (KeyRef != s.toHexString(s.key)) {
  console.log("FAILURE in key: ");
  console.log(KeyRef, " != ");
  console.log(s.toHexString(s.key));
}


