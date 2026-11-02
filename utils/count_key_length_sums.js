const fs = require("node:fs");
const path = require("node:path");

const inputPath = process.argv[2];
if (!inputPath) {
  console.error(`Usage: ${process.argv[0]} ${process.argv[1]} <file.json|directory>`);
  process.exit(1);
}

const counts = new Map();

function visit(value) {
  if (Array.isArray(value)) {
    for (const item of value) visit(item);
  } else if (value !== null && typeof value === "object") {
    const summaryLength = Object.keys(value).reduce(
      (total, key) => total + key.length,
      0
    );

    counts.set(summaryLength, (counts.get(summaryLength) ?? 0) + 1);

    for (const child of Object.values(value)) visit(child);
  }
}

function processJsonFile(filePath) {
	console.error("Processing " + filePath);
	try {
		const data = JSON.parse(fs.readFileSync(filePath, "utf8"));
  	visit(data);
	} catch(e){
		console.error("Failed to process " + filePath + ": " + e);
	}
}

function processPath(targetPath) {
  const stats = fs.statSync(targetPath);

  if (stats.isDirectory()) {
    for (const entry of fs.readdirSync(targetPath, { withFileTypes: true })) {
      const entryPath = path.join(targetPath, entry.name);

      if (entry.isDirectory()) {
        processPath(entryPath);
      } else if (entry.isFile() && path.extname(entry.name).toLowerCase() === ".json") {
        processJsonFile(entryPath);
      }
    }
  } else if (stats.isFile()) {
    if (path.extname(targetPath).toLowerCase() !== ".json") {
      throw new Error("The input file must have a .json extension.");
    }
    processJsonFile(targetPath);
  } else {
    throw new Error("Input path must be a JSON file or directory.");
  }
}

try {
  processPath(inputPath);

  const output = [...counts.entries()]
    .sort(([lengthA], [lengthB]) => lengthA - lengthB)
    .map(([length, objectCount]) => `(${length};${objectCount})`)
    .join(" ");

  console.log(output);
} catch (error) {
  console.error(`Error: ${error.message}`);
  process.exit(1);
}