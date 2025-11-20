import { NextRequest, NextResponse } from 'next/server';
import fs from 'fs';
import path from 'path';

export async function POST(request: NextRequest) {
  try {
    const { fileName, csvContent, overwrite } = await request.json();

    let finalFileName = fileName;
    if (!finalFileName.toLowerCase().endsWith('.csv')) {
      finalFileName += '.csv';
    }

    const saveDir = path.join(process.cwd(), 'saving_route');
    const filePath = path.join(saveDir, finalFileName);

    if (!fs.existsSync(saveDir)) {
      fs.mkdirSync(saveDir, { recursive: true });
    }

    if (fs.existsSync(filePath) && !overwrite) {
      return NextResponse.json({ status: 'exists', finalFileName });
    } else {
      fs.writeFileSync(filePath, csvContent, 'utf8');
      return NextResponse.json({ status: 'success', finalFileName });
    }
  } catch (err: any) {
    return NextResponse.json(
      { status: 'error', message: err.message },
      { status: 500 }
    );
  }
}

