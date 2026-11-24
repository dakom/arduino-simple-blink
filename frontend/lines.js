// Splits a stream of text into lines, the way a terminal would.
//
// "\r\n", "\n" and a lone "\r" each end a line. Text arrives in chunks of any
// size, so a "\r\n" can be split across two chunks: remember a trailing "\r" so
// the "\n" that follows it doesn't end a second, empty line.
export class LineSplitter {
  #afterCR = false;

  // Returns the pieces of `chunk` in order: { text, ends }, where `ends` means
  // the line is complete after `text`. A last piece with ends: false is the
  // start of a line that later chunks will continue.
  push(chunk) {
    const pieces = [];
    let start = 0;

    for (let i = 0; i < chunk.length; i++) {
      const char = chunk[i];

      if (char === "\n" && this.#afterCR) {
        // Second half of a "\r\n": the line already ended at the "\r".
        this.#afterCR = false;
        start = i + 1;
        continue;
      }
      this.#afterCR = false;

      if (char === "\r" || char === "\n") {
        pieces.push({ text: chunk.slice(start, i), ends: true });
        start = i + 1;
        this.#afterCR = char === "\r";
      }
    }

    if (start < chunk.length) {
      pieces.push({ text: chunk.slice(start), ends: false });
    }
    return pieces;
  }
}
