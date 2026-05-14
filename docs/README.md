# RTBEngine GitHub Pages Site

Static website for Epic Account Services Brand Settings.

## Publish With GitHub Pages

1. Push this repository to GitHub.
2. Open the GitHub repository settings.
3. Go to `Pages`.
4. Set source to `Deploy from a branch`.
5. Select branch `develop` and folder `/docs`.
6. Save and wait for GitHub Pages to publish.

Expected URLs:

```text
https://PabloGlezAlv.github.io/RTBEngine/
https://PabloGlezAlv.github.io/RTBEngine/privacy.html
```

Use the first URL as the Epic `Application Website` and the second URL as the
Epic `Privacy Policy URL`.

## Custom Domain With is-a.dev

This repo is prepared for the custom domain:

```text
https://www.rtbengine.is-a.dev/
https://www.rtbengine.is-a.dev/privacy.html
https://www.rtbengine.is-a.dev/terms.html
```

The `CNAME` file in this folder tells GitHub Pages to use
`www.rtbengine.is-a.dev`.

The DNS files to submit to `is-a-dev/register` are prepared in:

```text
../is-a-dev/domains/rtbengine.json
../is-a-dev/domains/www.rtbengine.json
```

After the `is-a.dev` pull request is merged, open GitHub repository settings,
go to `Pages`, and set the custom domain to:

```text
www.rtbengine.is-a.dev
```

Then use these URLs in Epic Brand Settings:

```text
Application Website:
https://www.rtbengine.is-a.dev/

Privacy Policy URL:
https://www.rtbengine.is-a.dev/privacy.html

Terms URL, if requested:
https://www.rtbengine.is-a.dev/terms.html
```
