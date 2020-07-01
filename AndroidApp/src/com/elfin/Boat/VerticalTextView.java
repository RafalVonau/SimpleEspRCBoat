package com.elfin.Boat;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.text.TextPaint;
import android.util.AttributeSet;
import android.widget.TextView;

public class VerticalTextView extends TextView 
{
	public VerticalTextView(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
	}

	public VerticalTextView(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	public VerticalTextView(Context context) {
		super(context);
	}

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		super.onMeasure( heightMeasureSpec, widthMeasureSpec );
		setMeasuredDimension( getMeasuredHeight(),getMeasuredWidth() );
	}

	@Override
	protected void onDraw(Canvas canvas) {
		TextPaint textPaint = getPaint();
		textPaint.setColor( getCurrentTextColor() );
		textPaint.drawableState = getDrawableState();
 
		canvas.save();
 
		canvas.translate( getWidth(), 0 );
		canvas.rotate( 90 );
 
		canvas.translate( getCompoundPaddingLeft(), getExtendedPaddingTop() );
 
		getLayout().draw( canvas );
		canvas.restore();
	}

	private String text() {
		return super.getText().toString();
	}
}