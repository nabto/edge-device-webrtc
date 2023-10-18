import { TestDevice } from "./fixture";

const device = new TestDevice();

export async function mochaGlobalSetup() {
  device.start(true);
}

export async function mochaGlobalTeardown() {
  device.stop();
}
