// Do not turn missing or ambiguous wiki evidence into an "optional" claim.
export function keyNeed(body, name) {
  if (!name) return "unknown";
  const sentences = String(body || "").toLowerCase().split(/(?<=[.!?])\s+|\n/).filter(s => s.includes(name.toLowerCase()));
  const no = /\boptional\b|\bnot (?:be )?(?:required|needed|necessary)\b|\bisn't (?:required|needed)\b|\bwithout (?:the |a |its |using )?(?:key|it)\b/;
  const yes = /\b(?:is|are) (?:required|needed|necessary)\b|\brequires?\b|\bneed(?:s|ed)? (?:the|a|an|to)\b|\byou(?:'ll| will)? (?:need|must)\b|\bmust (?:have|use|unlock)\b/;
  const optional = sentences.some(s => no.test(s)), required = sentences.some(s => yes.test(s) && !no.test(s));
  if (optional && !required) return "optional";
  if (required && !optional) return "required";
  return "unknown";
}
