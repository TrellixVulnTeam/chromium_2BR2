<!-- Copyright 2015 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file.
-->
# Contributing to Catapult

## Contributing

Install [depot_tools]
(https://www.chromium.org/developers/how-tos/install-depot-tools).

Then checkout the catapult repo.

`git clone https://github.com/catapult-project/catapult.git`

You can then create a local branch, make and commit your change.

```
cd catapult
git checkout -t -b foo origin/master
... edit files ...
git commit -a -m "New files"
```

Once you're ready for a review do:

`git cl upload`

Once uploaded you can view the CL in Rietveld and request a review by clicking
the 'publish & mail' link. You can also click the "CQ Dry Run" link to run all
the tests on your change.

If you get review feedback, edit and commit locally and then do another upload
with the new files. Before you commit you'll want to sync to the tip-of-tree.
You can either merge or rebase, it's up to you.

Then, submit your changes through the commit queue by checking the "Commit" box.

Once everything is landed, you can cleanup your branch.

```
git checkout master
git branch -D foo
```

## Legal

If you're new to the chromium-family of projects, you will also need to sign the
chrome contributors license agreement. You can sign the
[Contributor License Agreement]
(https://cla.developers.google.com/about/google-individual?csw=1), which you can
do online.
It only takes a minute. If you are contributing on behalf of a corporation, you
must fill out the [Corporate Contributor License Agreement]
(https://cla.developers.google.com/about/google-corporate?csw=1) and send it to
us as described on that page.

If you've never submitted code before, you must add your (or your
organization's) name and contact info to the Chromium AUTHORS file.

## Contributing from a Chromium checkout

If you already have catapult checked out as part of a Chromium checkout and want
to edit it in place (instead of having a separate clone of the repository), you
will probably want to disconnect it from gclient at this point so that it
doesn't do strange things on updating. This is done by editing the .gclient file
for your Chromium checkout and adding the following lines:

```
'custom_deps': {
    'src/third_party/catapult': None,
},
```

In order to be able to land patches, you will most likely need to update the
`origin` remote in your catapult checkout to point directly to this GitHub
repository. You can do this by executing the following command inside the
catapult folder (third_party/catapult):

`git remote set-url origin git@github.com:catapult-project/catapult`

## Code style

We follow the [Chromium style]
(https://www.chromium.org/developers/coding-style).

## Tests

Check individual project documentation for instructions on how to run tests.
You can also check the current status of our tests on the
[waterfall](http://build.chromium.org/p/client.catapult/waterfall).
Use the "commit" checkbox in rietveld to commit through the commit queue, which
automatically runs all tests. Run the tests before committing with the
"CQ dry run" link.

## Updating Chromium's about:tracing (rolling DEPS)

To get your change to appear in Chrome's about:tracing or other
third_party/catapult files, commit to catapult. Then check the [mirror]
(https://chromium.googlesource.com/external/github.com/catapult-project/catapult.git)
to find the git hash of your commit. (Note: it may take a few minutes to be
mirrored).

Then edit Chrome's [src/DEPS]
(https://code.google.com/p/chromium/codesearch#chromium/src/DEPS) file. Look for
a line like:

```
  'src/third_party/catapult':
    Var('chromium_git') + '/external/github.com/catapult-project/catapult.git' + '@' +
    '2da8924915bd6fb7609c518f5b1f63cb606248eb',
```

Update the number to the git hash you want to roll to, and [contribute a
codereview to chrome](http://www.chromium.org/developers/contributing-code)
for your edit. If you are a Chromium committer, feel free to TBR this.