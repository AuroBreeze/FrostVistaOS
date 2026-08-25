# Website Data

This directory contains generated, machine-readable data consumed by the
FrostVista OS website.

## Rules

- `releases.md` is the human-readable source of truth for the roadmap.
- GitHub Actions regenerates `roadmap.json` whenever `releases.md` changes.
- To regenerate locally, run `python3 scripts/generate_website_data.py`.
- Do not edit `roadmap.json` manually; it is a generated file.
- Keep stable `id` values for phases and roadmap items in `releases.md`.
- Use `planned`, `in_progress`, `complete`, or `blocked` for item status.
- Do not store a manually calculated percentage. The website calculates
  progress from the number of `complete` items.

## Files

- `roadmap.json`: generated current milestone, phase status, item status,
  descriptions, and validation commands for the roadmap page.

The website may fetch these files from the repository's GitHub raw URL and
should provide a bundled fallback for unavailable network requests.

## Automation

The `generate website data` workflow runs on pushes to `main` or `dev` that
change `releases.md` or the generator. It commits the generated JSON back to
the branch that triggered the workflow, so roadmap updates do not require a
manual generation step.
