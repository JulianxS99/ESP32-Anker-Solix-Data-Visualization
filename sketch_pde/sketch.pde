// ================== Layout variables =========================
int w = 320, h = 240;

// --- Graph area ---
int graphX      = 40;
int graphY      = 8;
int graphWidth  = 270;
int graphHeight = 120;

// --- Legend (position inside the graph) ---
int legendBoxW = 115;
int legendBoxH = 38;
int legendBoxMarginX = 8;
int legendBoxMarginY = 6;
int legendColorBox = 12;
int legendTextOffsetY = 7;

// --- Value block (Battery, Generated, Consumed) ---
int valuesX      = 5;
int valuesY      = 150;
int valueLabelToValDist = 160;
int rowHeight    = 24;

// --- Refresh button ---
int refreshBtnW = 80, refreshBtnH = 30;
int refreshBtnX = w - refreshBtnW - 10;
int refreshBtnY = 200;
int refreshTextOffsetX = 16;
int refreshTextOffsetY = -1;

// --- Timestamp ---
int updatedY = 220;

// ================== End of layout variables ====================

int POINTS_PER_DAY = 24;
float[] generation = new float[POINTS_PER_DAY];
float[] consumption = new float[POINTS_PER_DAY];
float batteryPercent = 78.4;
float dailyGen = 4.21;
float dailyCons = 3.14;
String lastUpdateStr = "14:23:05";

PFont font;
int fontBig = 16;
int fontSmall = 12;

void setup() {
  windowResize(w, h);
  font = createFont("Arial", fontBig);
  textFont(font);

  for (int i = 0; i < POINTS_PER_DAY; i++) {
    generation[i]  = 40 + 30 * sin(map(i, 0, 23, -0.6, PI));
    consumption[i] = 30 + 15 * cos(map(i, 0, 23, 0, PI));
    generation[i] += random(-5,5);
    consumption[i] += random(-3,3);
  }
}

void draw() {
  background(20);
  drawGraph(generation, consumption);
  drawNumbers(batteryPercent, dailyGen, dailyCons);
  drawRefreshButton();
  drawUpdatedText();
}

// -------- Graph with automatic scaling; generation is thick and yellow --------
void drawGraph(float[] genData, float[] consData) {
  int x0 = graphX, y0 = graphY, gw = graphWidth, gh = graphHeight;

  // Y-axis scaling
  float dataMax = max(arrayMax(genData), arrayMax(consData));
  float maxVal = roundUpToNiceValue(dataMax);

  // Grid
  int gridLines = 5;
  float gridStep = maxVal / (gridLines-1);

  stroke(200); noFill();
  rect(x0, y0, gw, gh);

  textFont(font, fontSmall);
  for (int i = 0; i < gridLines; i++) {
    int y = y0 + gh - int(gh * i / float(gridLines-1));
    stroke(160, 220, 255, 170);
    strokeWeight(i==0 ? 2 : 1);
    line(x0, y, x0+gw, y);
    fill(255); noStroke();
    textAlign(RIGHT, CENTER);
    int gridVal = int(i * gridStep + 0.5);
    text(str(gridVal), x0-8, y);
  }
  strokeWeight(1);

  // X-axis
  for (int i=0; i<=24; i+=6) {
    int x = x0 + (gw * i) / POINTS_PER_DAY;
    stroke(120,180,255,80);
    line(x, y0, x, y0+gh);
    fill(255); noStroke();
    textFont(font, fontSmall);
    textAlign(CENTER, TOP);
    text(str(i), x, y0 + gh + 2);
  }

  // Generation curve (thick yellow)
  stroke(255,215,0); strokeWeight(3); noFill();
  beginShape();
  for (int i=0; i<POINTS_PER_DAY; i++) {
    float x = x0 + gw*i/(POINTS_PER_DAY-1);
    float y = y0 + gh - (genData[i]/maxVal) * gh;
    vertex(x, y);
  }
  endShape();
  strokeWeight(1);

  // Consumption curve (red)
  stroke(255, 60, 60); noFill();
  beginShape();
  for (int i=0; i<POINTS_PER_DAY; i++) {
    float x = x0 + gw*i/(POINTS_PER_DAY-1);
    float y = y0 + gh - (consData[i]/maxVal) * gh;
    vertex(x, y);
  }
  endShape();

  // Legend in the top-right, generation is yellow
  int legendX = x0 + gw - legendBoxW - legendBoxMarginX;
  int legendY = y0 + legendBoxMarginY;
  fill(30, 200);  noStroke();
  rect(legendX, legendY, legendBoxW, legendBoxH, 6);

  // Generation (top)
  int boxY = legendY + 6;
  fill(255,215,0); rect(legendX+8, boxY, legendColorBox, legendColorBox, 2);
  fill(255); textFont(font, fontSmall); textAlign(LEFT, TOP);
  text("Generation", legendX+8+legendColorBox+6, boxY - 2 + legendTextOffsetY);

  // Consumption (bottom)
  fill(255,60,60); rect(legendX+8, boxY+legendColorBox+4, legendColorBox, legendColorBox, 2);
  fill(255);
  text("Consumption", legendX+8+legendColorBox+6, boxY+legendColorBox+2 + legendTextOffsetY);
}

float arrayMax(float[] arr) {
  float m = arr[0];
  for (int i=1; i<arr.length; i++) if (arr[i] > m) m = arr[i];
  return m;
}
float roundUpToNiceValue(float v) {
  float[] nice = {10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 5000};
  for (int i=0; i<nice.length; i++) if (v <= nice[i]) return nice[i];
  float base = pow(10, floor(log(v)/log(10)));
  return ceil(v/base)*base;
}

void drawNumbers(float batteryPercent, float dailyGen, float dailyCons) {
  int colX = valuesX, startY = valuesY;

  textFont(font, fontBig); fill(255); textAlign(LEFT, TOP);

  text("Battery:", colX, startY);
  textAlign(RIGHT, TOP);
  text(nf(batteryPercent,0,1)+" %", colX + valueLabelToValDist, startY);

  textAlign(LEFT, TOP);
  text("Generated:", colX, startY + rowHeight);
  textAlign(RIGHT, TOP);
  text(nf(dailyGen,0,2)+" kWh", colX + valueLabelToValDist, startY + rowHeight);

  textAlign(LEFT, TOP);
  text("Consumed:", colX, startY + 2*rowHeight);
  textAlign(RIGHT, TOP);
  text(nf(dailyCons,0,2)+" kWh", colX + valueLabelToValDist, startY + 2*rowHeight);

  textFont(font, fontSmall); textAlign(LEFT, TOP);
}

void drawRefreshButton() {
  fill(40); stroke(180);
  rect(refreshBtnX, refreshBtnY, refreshBtnW, refreshBtnH, 6);
  noStroke(); fill(255);
  textFont(font, fontBig-2);
  textAlign(LEFT, CENTER);
  text("Refresh", refreshBtnX + refreshTextOffsetX, refreshBtnY + refreshBtnH/2 + refreshTextOffsetY);
}

void drawUpdatedText() {
  fill(180); textFont(font, fontSmall); textAlign(LEFT, TOP);
  text("Updated: "+lastUpdateStr, valuesX, updatedY);
}
