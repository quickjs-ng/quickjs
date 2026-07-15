new Set([{}]).union(new Set([9]));                 // leaked the {} element
new Set([{}]).symmetricDifference(new Set([9]));   // same

// symmetricDifference: element only reachable via the setlike iterator
new Set([1]).symmetricDifference(new Set([{}]));

// intersection, |this| larger than the argument: iterates the argument's keys
var o1 = {};
new Set([o1, 1, 2]).intersection(new Set([o1]));

// intersection, |this| not larger: iterates |this| and probes with .has()
var o2 = {};
new Set([o2]).intersection(new Set([o2, 1, 2]));

// difference, |this| not larger: iterates |this| and keeps the misses
new Set([{}]).difference(new Set([9]));
