#!/bin/bash
#
# release.sh - Release management for basica
#
# Usage:
#   ./release.sh              Show current version and status
#   ./release.sh tag          Tag the current commit and push
#   ./release.sh package      Build the distribution zip
#   ./release.sh release      Create GitHub release and upload assets
#   ./release.sh formula      Update the Homebrew formula with the correct version and SHA256
#   ./release.sh all          Run all steps: tag, package, release, and formula update
#
# Requires: gh (GitHub CLI), authenticated via `gh auth login`
#

set -euo pipefail

VERSION=$(grep 'BASICA_VERSION' src/common.h | head -1 | sed 's/.*"\(.*\)".*/\1/')
TAG="v${VERSION}"
FORMULA="Formula/basica.rb"
PACKAGE_NAME="basica-${VERSION}"
ARCHIVE_URL="https://github.com/kdekorte/basica/releases/download/${TAG}/${PACKAGE_NAME}.zip"

info()    { printf "\033[1;34m==>\033[0m \033[1m%s\033[0m\n" "$1"; }
success() { printf "\033[1;32m==>\033[0m \033[1m%s\033[0m\n" "$1"; }
error()   { printf "\033[1;31mError:\033[0m %s\n" "$1" >&2; exit 1; }

show_status() {
    info "Basica release status"
    echo "  Version:      ${VERSION}"
    echo "  Tag:          ${TAG}"
    echo "  Formula:      ${FORMULA}"
    echo "  Archive URL:  ${ARCHIVE_URL}"
    echo ""

    if git rev-parse "${TAG}" >/dev/null 2>&1; then
        success "Tag ${TAG} exists locally"
    else
        echo "  Tag ${TAG} has not been created yet"
    fi

    if command -v gh >/dev/null 2>&1; then
        if gh release view "${TAG}" >/dev/null 2>&1; then
            success "GitHub release ${TAG} exists"
        else
            echo "  GitHub release ${TAG} has not been created yet"
        fi
    fi
}

do_tag() {
    info "Tagging release ${TAG}"

    if git rev-parse "${TAG}" >/dev/null 2>&1; then
        error "Tag ${TAG} already exists. Bump BASICA_VERSION in src/common.h first."
    fi

    if ! git diff --quiet || ! git diff --cached --quiet; then
        error "Working tree is dirty. Commit all changes before tagging."
    fi

    git tag -a "${TAG}" -m "Release ${VERSION}"
    info "Pushing tag ${TAG} to origin"
    git push origin "${TAG}"
    success "Tag ${TAG} created and pushed"
}

do_package() {
    info "Building distribution package"
    make package
    success "Created ${PACKAGE_NAME}.zip"
}

do_release() {
    info "Creating GitHub release ${TAG}"

    command -v gh >/dev/null 2>&1 || error "GitHub CLI (gh) is required. Install with: brew install gh"
    gh auth status >/dev/null 2>&1 || error "Not authenticated. Run: gh auth login"

    if ! git rev-parse "${TAG}" >/dev/null 2>&1; then
        error "Tag ${TAG} does not exist. Run './release.sh tag' first."
    fi

    # Build the binary
    info "Building basica binary"
    make clean
    make
    if [ ! -f basica ]; then
        error "Binary 'basica' not found after build."
    fi

    # Build the source package
    info "Building source package"
    make package
    if [ ! -f "${PACKAGE_NAME}.zip" ]; then
        error "Package '${PACKAGE_NAME}.zip' not found after build."
    fi

    # Create the release (or skip if it already exists)
    if gh release view "${TAG}" >/dev/null 2>&1; then
        info "GitHub release ${TAG} already exists, uploading assets (overwriting if present)"
        gh release upload "${TAG}" \
            "${PACKAGE_NAME}.zip" \
            basica \
            --clobber
    else
        info "Creating new GitHub release ${TAG}"
        gh release create "${TAG}" \
            "${PACKAGE_NAME}.zip" \
            basica \
            --title "Basica ${VERSION}" \
            --generate-notes
    fi

    success "GitHub release ${TAG} created with assets:"
    echo "  - ${PACKAGE_NAME}.zip  (source package)"
    echo "  - basica              (macOS binary)"

    # Clean up build artifacts
    info "Cleaning up build artifacts"
    make clean
    rm -f "${PACKAGE_NAME}.zip"
}

do_formula() {
    info "Updating Homebrew formula for ${TAG}"

    if ! git rev-parse "${TAG}" >/dev/null 2>&1; then
        error "Tag ${TAG} does not exist. Run './release.sh tag' first."
    fi

    # Download the archive and compute SHA256
    info "Downloading archive to compute SHA256..."
    SHA256=$(curl -sL "${ARCHIVE_URL}" | shasum -a 256 | awk '{print $1}')

    if [ -z "${SHA256}" ] || [ "${#SHA256}" -ne 64 ]; then
        error "Failed to compute SHA256. Has the release been published? Run './release.sh release' first."
    fi

    info "SHA256: ${SHA256}"

    # Update the URL version
    sed -i '' "s|url \"https://github.com/kdekorte/basica/releases/download/v[^/]*/basica-.*\.zip\"|url \"${ARCHIVE_URL}\"|" "${FORMULA}"

    # Update or insert sha256 line
    if grep -q '# sha256' "${FORMULA}"; then
        # Replace commented-out placeholder
        sed -i '' "s|# sha256 .*|sha256 \"${SHA256}\"|" "${FORMULA}"
    elif grep -q 'sha256' "${FORMULA}"; then
        # Replace existing sha256
        sed -i '' "s|sha256 \".*\"|sha256 \"${SHA256}\"|" "${FORMULA}"
    else
        # Insert after the url line
        sed -i '' "/^  url /a\\
  sha256 \"${SHA256}\"" "${FORMULA}"
    fi

    success "Formula updated:"
    echo "  URL:    ${ARCHIVE_URL}"
    echo "  SHA256: ${SHA256}"
    echo ""
    echo "Don't forget to commit the updated formula:"
    echo "  git add ${FORMULA}"
    echo "  git commit -m \"Update formula for ${VERSION}\""
    echo "  git push"
}

do_all() {
    do_tag
    echo ""
    do_release
    echo ""
    do_formula
}

# --- Main ---

case "${1:-status}" in
    status)   show_status ;;
    tag)      do_tag ;;
    package)  do_package ;;
    release)  do_release ;;
    formula)  do_formula ;;
    all)      do_all ;;
    *)
        echo "Usage: $0 {status|tag|package|release|formula|all}"
        echo ""
        echo "Commands:"
        echo "  status    Show current version and release status (default)"
        echo "  tag       Create and push a git tag for the current version"
        echo "  package   Build the basica-VERSION.zip distribution archive"
        echo "  release   Create GitHub release and upload source + binary"
        echo "  formula   Update the Homebrew formula URL and SHA256"
        echo "  all       Run tag, release, and formula in sequence"
        exit 1
        ;;
esac
