let it;
const evil = {
  [Symbol.iterator]() {
    it.return();
    return [][Symbol.iterator]();
  }
};
it = Iterator.concat(evil);
it.next();
