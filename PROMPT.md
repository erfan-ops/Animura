# Animura Documentation Synchronization Task

## Objective

Your task is to update the Animura documentation after the frontend rewrite.

The project has changed significantly, especially around:

- Frontend architecture.
- UI system.
- WebView2 integration.
- React application structure.
- Native ↔ frontend communication.
- User interface workflow.

Your job is to make the documentation accurately represent the **current state of the project**.

---

# Important Restrictions

## Only Modify Documentation

You may ONLY change:

```

docs/
ai-docs/

```

Allowed:

- Create new markdown files.
- Delete outdated documentation files.
- Rename documentation files.
- Rewrite existing documentation.
- Update references.

Forbidden:

- Do not modify source code.
- Do not modify build files.
- Do not modify configuration files.
- Do not modify frontend files.
- Do not modify backend files.

Your output should only contain documentation changes.

---

# Phase 1 — Understand Existing Documentation

Before changing anything:

## Step 1: Read All Documentation

Read completely:

```

docs/

```

and:

```

ai-docs/

```

Do not skip files.

Understand:

- Previous architecture.
- Previous UI system.
- Previous workflows.
- Existing terminology.
- Existing documentation structure.
- Deprecated information.

Create an internal map:

```

Current Documentation

├── Valid Information
│
├── Outdated Information
│
├── Missing Information
│
└── Needs Rewrite

```

Do not edit yet.

---

# Phase 2 — Analyze Current Project State

After understanding the old documentation:

Inspect the current project state.

Focus on changes introduced since the last commit.

Identify:

- New frontend structure.
- Removed UI systems.
- New React components.
- WebView2 integration.
- Frontend build workflow.
- Communication layer.
- Changed application flow.
- Changed architecture.

Compare:

```

Previous Architecture

vs

Current Architecture

```

Document:

- What was replaced.
- What was added.
- What was removed.
- What changed behavior.

---

# Phase 3 — Update docs/

Update:

```

docs/

```

This documentation should describe the project for developers.

Update all relevant sections.

Especially:

## UI Documentation

Replace old UI information with:

- React frontend architecture.
- WebView2 host.
- Frontend structure.
- Component organization.
- UI workflow.

---

## Architecture Documentation

Update:

Old:

```

Native Application
|
UI

```

to the new architecture:

```

C++ Application

```
    |
```

WebView2 Host

```
    |
```

React Frontend

```

Explain:

- Responsibilities of each layer.
- Communication between layers.
- Data flow.

---

## Development Workflow

Update:

Include:

- Frontend development setup.
- npm commands.
- Frontend build process.
- WebView2 workflow.
- Native build workflow.

---

## Remove Deprecated Information

Delete or rewrite documentation describing:

- Old UI framework.
- Removed UI components.
- Old rendering/UI communication.
- Old workflows.

Do not leave conflicting information.

---

# Phase 4 — Update ai-docs/

Update:

```

ai-docs/

```

This documentation is for AI coding agents.

It must reflect the new architecture.

---

# Required AI Documentation Updates

Ensure these topics exist:

## Frontend Architecture

Document:

- React application structure.
- Component hierarchy.
- State management.
- UI patterns.
- Styling system.

---

## WebView2 Integration

Document:

- How WebView2 hosts React.
- How communication works.
- Native bridge design.
- Message flow.

Example:

```

React

↓

JavaScript Bridge

↓

C++

↓

Animura Backend

```

---

## Frontend Source Guide

Document:

- Important folders.
- Important components.
- Where new UI features should be added.

Example:

```

frontend/

src/

components/
Contains reusable UI components.

bridge/
Contains C++ communication layer.

```

---

## UI Development Guide

Document:

- How to add a new component.
- How to add a new module card.
- How to modify settings UI.
- How to maintain the design system.

---

## Migration History

Create or update a document explaining:

- What changed.
- Why it changed.
- Old vs new architecture.

Example:

```

Before:

Native UI

After:

React + WebView2

```

---

# Documentation Cleanup

Perform a full cleanup.

Remove:

- Duplicate documents.
- Incorrect references.
- Old file paths.
- Deprecated architecture descriptions.

Fix:

- Broken links.
- Wrong examples.
- Outdated diagrams.

---

# Documentation Structure

You are free to improve the structure.

You may:

Add:

```

ai-docs/frontend/

```

or:

```

docs/frontend/

```

if it improves organization.

Example:

```

ai-docs/

├── README.md
├── architecture.md
├── frontend/
│   ├── react-architecture.md
│   ├── webview2-bridge.md
│   └── ui-development.md
│
└── backend/
└── ...

```

Organize logically.

---

# Final Validation

Before finishing verify:

## Accuracy

- Documentation matches current source structure.
- No old UI information remains.
- New frontend architecture is fully explained.

## AI Usability

An AI agent should be able to:

1. Read `ai-docs/README.md`
2. Understand the current architecture
3. Find frontend-related documentation
4. Modify UI safely

## Consistency

Check:

- File references.
- Links.
- Folder names.
- Terminology.

---

# Final Output

Only documentation changes.

Modified:

```

docs/
ai-docs/

```

No source code changes.

The final documentation should represent the current Animura project after the frontend rewrite.
