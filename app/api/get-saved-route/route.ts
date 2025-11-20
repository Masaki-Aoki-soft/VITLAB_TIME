import { NextRequest, NextResponse } from 'next/server';
import fs from 'fs';
import path from 'path';

export async function GET(request: NextRequest) {
  const searchParams = request.nextUrl.searchParams;
  const fileName = searchParams.get('fileName');

  if (!fileName) {
    return NextResponse.json(
      { error: 'File name is required' },
      { status: 400 }
    );
  }

  const filePath = path.join(process.cwd(), 'saving_route', fileName);
  if (fs.existsSync(filePath)) {
    const data = fs.readFileSync(filePath, 'utf8');
    return new NextResponse(data, {
      status: 200,
      headers: {
        'Content-Type': 'text/csv; charset=utf-8',
      },
    });
  } else {
    return NextResponse.json({ error: 'File not found' }, { status: 404 });
  }
}

