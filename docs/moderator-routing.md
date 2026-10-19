<!-- This software is copyrighted as detailed in the LICENSE file. -->
<!-- Copyright (c) 2026, Richard Thomson -->

# Moderator Routing

Each access-file news source accepts `Moderators File`:

- A file name selects a local INN-style moderators file.  Normal trn path
  expansion applies.  This overrides remote lookup for that source.
- An omitted or empty value uses `moderators` beside a local active file.
- An NNTP source with an omitted, empty, or `remote` value uses
  `LIST MODERATORS` when the server advertises `LIST MODERATORS` through
  `CAPABILITIES`.  Legacy and unadvertised lists are not probed.
- `none` disables client moderator lookup.

Records follow [INN moderators][inn] and [RFC 6048, section 2.4][rfc]:

```text
lists.project:project-list@example.org
local.*:%s@example.org
*:%s@moderators.example.org
```

Blank lines and comments beginning with `#` are ignored.  Rules retain
file order; the first matching valid rule wins.  Patterns use wildmat
matching, including comma-separated alternatives and `!` exclusions.
One `%s` may occur in an address template.  It expands to the newsgroup
name with dots changed to dashes; `%%` produces a literal percent sign.
Addresses must be simple ASCII mailboxes without whitespace, controls,
angle brackets, or commas.  Malformed records are ignored.

Local files are reread for each lookup.  Remote replies must finish with
the NNTP dot terminator before any matching address is used.  Missing
files, rejected commands, incomplete replies, and unmatched groups fall
back to the existing news posting path.

The outgoing `Newsgroups` header determines routing, including folded
values and crossposts.  Only groups marked `m` in the source's active
file are routed.  Posts with an `Approved` value use normal news posting.
Matched destinations are sorted and deduplicated into `To`; any previous
`To` header and its continuations are replaced.  Other headers, including
`Newsgroups`, and the article body are retained.  The configured
`MAILPOSTER` opens the mail composer.  Its result does not trigger a
second submission through `NEWSPOSTER`.

For a mailing list presented as a local spool, configure its source:

```ini
[project-list]
Active File = ~/news/active
Spool Dir = ~/news/spool
Moderators File = ~/news/moderators
```

The active file needs a moderated entry such as `lists.project 1 1 m`.
The first rule above supplies the list address.  Use `f` or `F` to post or
follow up to the list.  Private `r`/`R` replies retain their usual
recipient.

`ModeratorPatternTest`, `ModeratorSourceTest`, `DataSourceConfigTest`,
`DataSourceInitTest`, and `ModeratorPostingTest` cover rules, source
selection, network replies, configuration, and composer invocation.  The
tests use local files, mock NNTP connections, and harmless composer stubs.

[inn]: https://www.eyrie.org/~eagle/software/inn/docs/moderators.html
[rfc]: https://www.ietf.org/rfc/rfc6048.txt
