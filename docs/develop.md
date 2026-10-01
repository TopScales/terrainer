# Introduction

Keeping up-to-date documentation is critically important in video game development, because the process can be highly collaborative, complex, and often subject to change. Documentation, especially the Game Design Document (GDD), acts as the central, single source of truth that guides the entire team. When documentation is treated as an integral asset-updated alongside the codebase, it becomes a reliable reference for contributors, testers, and stakeholders.

Having clear and accessible documentation from the beginning of the project is crucial to have a shared understanding of the goals and expectation of the game. This will allow for new team members to get on-board faster and to better understand the project's vision.

Storing the documentation within the same repository as the game project provides several advantages:

- Version Control Alignment: Documentation evolves in direct sync with code changes, ensuring consistency across revisions.
- Reduced Context Switching: Contributors can access game assets, scripts, and documentation from a unified location.
- Simplified Collaboration: Pull requests, issue tracking, and reviews can include updates to both code and documentation.
- Long-Term Maintainability: Historical documentation remains accessible, improving traceability.

The current documentation makes use of [Material](https://squidfunk.github.io/mkdocs-material/) for [MkDocs](https://www.mkdocs.org/). Make sure that all team members are familiar with how documents are created using this framework.

## How to use the docs

### Local Installation

To render this documentation locally, install MkDocs and the Material for MkDocs theme.

#### Prerequisites

* Python 3.7 or later
* pip (Python package installer)

#### Installation Steps

1. It is preferred to install Material framework in a virtual environment:
    ```powershell
    python -m venv venv
    .\venv\Scripts\activate.bat
    ```
2. Install the Material for MkDocs theme, and plugins:
    ```powershell
    pip install mkdocs-material
    pip install mkdocs-awesome-nav
    ```
3. To view the documentation locally, navigate to the project root (where `mkdocs.yml` is located) and run:
   ```powershell
   mkdocs serve
   ```

The local documentation site refreshes automatically whenever changes are made to any file inside the `docs/` directory. This enables rapid iteration and a smooth editing experience.

>📝 **NOTE:** If the docs are not updating automatically, it is recommended to install `click` version less than 8.3.0, like `pip install "click==8.2.1"`.

## Setting Up VSCode

Although the project can be opened in any editor, the best experience is achieved with Visual Studio Code (VSCode).

To improve editing and previewing MkDocs content, install **MkDocs Syntax Highlight** extension. This extension enhances YAML and markdown editing, especially for `mkdocs.yml`.

To enable schema validation and improved autocompletion for Material extensions, add the following configuration to your VSCode `settings.json`:

```json
"yaml.schemas": {
    "https://squidfunk.github.io/mkdocs-material/schema.json": "mkdocs.yml"
},
"yaml.customTags": [
    "!ENV scalar",
    "!ENV sequence",
    "!relative scalar",
    "tag:yaml.org,2002:python/name:material.extensions.emoji.to_svg",
    "tag:yaml.org,2002:python/name:material.extensions.emoji.twemoji",
    "tag:yaml.org,2002:python/name:pymdownx.superfences.fence_code_format"
]
```

This configuration improves validation, provides clearer error messages, and enhances the editing workflow for Material-enhanced MkDocs projects.
