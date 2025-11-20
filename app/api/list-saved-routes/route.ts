import { NextResponse } from 'next/server';
import fs from 'fs';
import path from 'path';

export async function GET() {
  const saveDir = path.join(process.cwd(), 'saving_route');
  if (!fs.existsSync(saveDir)) {
    return NextResponse.json([]);
  }

  try {
    const files = fs.readdirSync(saveDir);
    const csvFiles = files.filter((file) => file.toLowerCase().endsWith('.csv'));
    return NextResponse.json(csvFiles);
  } catch (err: any) {
    return NextResponse.json(
      { error: 'Failed to list files' },
      { status: 500 }
    );
  }
}

