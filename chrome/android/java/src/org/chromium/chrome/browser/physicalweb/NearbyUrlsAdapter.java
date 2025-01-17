// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * Adapter for displaying nearby URLs and associated metadata.
 */
class NearbyUrlsAdapter extends ArrayAdapter<PwsResult> {

    /**
     * Construct an empty NearbyUrlsAdapter.
     */
    public NearbyUrlsAdapter(Context context) {
        super(context, 0);
    }

    /**
     * Get the view for an item in the data set.
     * @param position Index of the list view item within the array.
     * @param view The old view to reuse, if possible.
     * @param viewGroup The parent that this view will eventually be attached to.
     * @return A view corresponding to the list view item.
     */
    @Override
    public View getView(int position, View view, ViewGroup viewGroup) {
        if (view == null) {
            LayoutInflater inflater = LayoutInflater.from(getContext());
            view = inflater.inflate(R.layout.physical_web_list_item_nearby_url, viewGroup, false);
        }

        TextView titleTextView = (TextView) view.findViewById(R.id.nearby_urls_title);
        TextView urlTextView = (TextView) view.findViewById(R.id.nearby_urls_url);
        TextView descriptionTextView = (TextView) view.findViewById(R.id.nearby_urls_description);
        ImageView iconImageView = (ImageView) view.findViewById(R.id.nearby_urls_icon);

        PwsResult pwsResult = getItem(position);
        titleTextView.setText(pwsResult.title);
        urlTextView.setText(pwsResult.siteUrl);
        descriptionTextView.setText(pwsResult.description);
        iconImageView.setImageBitmap(null);

        return view;
    }
}
