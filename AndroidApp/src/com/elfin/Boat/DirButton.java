package com.elfin.Boat;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.Build;
import android.util.AttributeSet;
import android.widget.ImageView;

public class DirButton extends ImageView {

private Context mContext;
private Paint paint;
private RectF rectf;
private int width;
private int height;

public DirButton(Context context) 
{
	super(context);
	init(context, null, 0);
}

public DirButton(Context context, AttributeSet attrs) 
{
	super(context, attrs);
	init(context, attrs, 0);
}

public DirButton(Context context, AttributeSet attrs, int defStyleAttr) 
{
	super(context, attrs, defStyleAttr);
	init(context, attrs, defStyleAttr);
}

private  void init(Context context, AttributeSet attrs, int defStyleAttr)
{
	mContext = context;
	paint = new Paint();
	rectf = new RectF();
	paint.setColor(Color.YELLOW);
	paint.setStyle(Paint.Style.FILL);
};

@Override
protected void onDraw(Canvas canvas) 
{
	rectf.set(0,0, width, height);
	canvas.drawRect(rectf, paint);
	//super.onDraw(canvas);
}

@Override
protected void onSizeChanged(int w, int h, int oldw, int oldh) 
{
	super.onSizeChanged(w, h, oldw, oldh);
	this.width = w;
	this.height = h;
}

public void setColor(int color)
{
	if (paint != null){
		paint.setColor(color);
		invalidate();
	}
}

}