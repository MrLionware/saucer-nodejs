# Publishing Guide for saucer-nodejs

## Prerequisites

- Node.js >= 20
- npm account (create at https://www.npmjs.com/signup)
- GitHub account

---

## Step 1: Update package.json

Before publishing, update these fields with your information:

```json
{
  "author": "Your Name <your.email@example.com>",
  "repository": {
    "type": "git",
    "url": "https://github.com/YOUR_USERNAME/saucer-nodejs.git"
  },
  "homepage": "https://github.com/YOUR_USERNAME/saucer-nodejs#readme",
  "bugs": {
    "url": "https://github.com/YOUR_USERNAME/saucer-nodejs/issues"
  }
}
```

---

## Step 2: Prepare Git Repository

```bash
# Navigate to project
cd /path/to/saucer-nodejs

# Initialize git (if needed)
git init

# Create .gitignore if needed
echo "node_modules/
build/
*.node
.DS_Store
*.log
.saucer-build/" > .gitignore

# Stage all files
git add .

# Create initial commit
git commit -m "Initial release v0.1.0

- Core webview functionality
- Clipboard API
- System notifications
- System tray (macOS)
- Window position, fullscreen, zoom
- CLI tools
- Debug logging
- Comprehensive test suite (200+ tests)"
```

---

## Step 3: Create GitHub Repository

1. Go to https://github.com/new
2. Repository name: `saucer-nodejs`
3. Description: "High-performance Node.js webview library with native system integration"
4. Public visibility
5. **Don't** initialize with README/LICENSE (you have them)
6. Click "Create repository"

---

## Step 4: Push to GitHub

```bash
# Add remote
git remote add origin https://github.com/YOUR_USERNAME/saucer-nodejs.git

# Push to main branch
git branch -M main
git push -u origin main
```

---

## Step 5: Create GitHub Release

1. Go to your repo → Releases → "Create a new release"
2. Tag: `v0.1.0`
3. Title: `v0.1.0 - Initial Release`
4. Description:
```markdown
## 🚀 Initial Release

### Features
- Non-blocking event loop with native Node.js integration
- Full webview capabilities (WebKit/WebView2/WebKitGTK)
- Clipboard API
- System Notifications
- System Tray (macOS)
- Window management (position, fullscreen, zoom)
- CLI tools (build, dev, doctor, init)
- Debug logging utilities
- TypeScript definitions
- 200+ automated tests

### Platform Support
- macOS (arm64, x64)
- Windows (x64, arm64)
- Linux (x64, arm64)

### Requirements
- Node.js >= 20.0.0
```
5. Click "Publish release"

---

## Step 6: Publish to npm

### First-time npm setup

```bash
# Login to npm (creates ~/.npmrc)
npm login

# Verify login
npm whoami
```

### Publish package

```bash
# Dry run first (see what would be published)
npm publish --dry-run

# If everything looks good, publish
npm publish

# For scoped packages (if using @org/package-name)
npm publish --access public
```

---

## Step 7: Verify Publication

```bash
# View your package on npm
open https://www.npmjs.com/package/saucer-nodejs

# Test installation in a new project
mkdir test-install && cd test-install
npm init -y
npm install saucer-nodejs

# Verify it works
node -e "const { Application } = require('saucer-nodejs'); console.log('Success!');"
```

---

## Publishing Updates

### Bump version

```bash
# Patch (0.1.0 → 0.1.1) - bug fixes
npm version patch

# Minor (0.1.0 → 0.2.0) - new features
npm version minor

# Major (0.1.0 → 1.0.0) - breaking changes
npm version major
```

### Publish update

```bash
# This automatically:
# 1. Updates package.json version
# 2. Creates git commit
# 3. Creates git tag

# Push to GitHub
git push && git push --tags

# Publish to npm
npm publish
```

---

## Pre-publish Checklist

Before each release:

- [ ] All tests pass: `echo "1" | node examples/application-features.js`
- [ ] Doctor passes: `node cli/saucer.js doctor`
- [ ] README is up to date
- [ ] CHANGELOG is updated (if you have one)
- [ ] Version is bumped in package.json
- [ ] No sensitive data in published files
- [ ] `npm pack --dry-run` shows correct files

---

## Troubleshooting

### "npm ERR! 403 Forbidden"
- Package name might be taken
- Try scoped name: `@your-username/saucer-nodejs`

### "npm ERR! You must be logged in"
```bash
npm login
```

### "Module not found" after install
- Ensure `"main"` in package.json points to correct file
- Check `"files"` array includes all needed files

### Prebuilt binaries not found
- Check `optionalDependencies` in package.json
- Ensure platform packages are published first

---

## Quick Commands Reference

```bash
# Check what will be published
npm pack --dry-run

# Publish to npm
npm publish

# Bump and publish
npm version patch && git push && git push --tags && npm publish

# Unpublish (within 72 hours, use carefully!)
npm unpublish saucer-nodejs@0.1.0

# Deprecate a version
npm deprecate saucer-nodejs@0.1.0 "Use version 0.2.0 instead"
```
